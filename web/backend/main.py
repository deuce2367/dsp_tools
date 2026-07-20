import os
import uuid
import glob
import shutil
import math
import datetime
import subprocess
from fastapi import FastAPI, BackgroundTasks, HTTPException, UploadFile, File
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import dsp_wrapper
import time
import asyncio
import collections
from typing import Optional

app = FastAPI(title="DSP Web Interface")

DATA_DIR = os.getenv("DSP_DATA_DIR", "/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")

memory_cache = collections.OrderedDict()

def add_to_cache(key: str, data: bytes):
    memory_cache[key] = data
    # Evict oldest if we have more than 50 cached files
    while len(memory_cache) > 50:
        memory_cache.popitem(last=False)

def parse_time_to_j1950(time_val):
    if not time_val:
        return 0.0
    try:
        val = float(time_val)
        if val > 2000000000:
            return val
        else:
            return val + 631152000
    except ValueError:
        try:
            dt_str = str(time_val).replace("Z", "+00:00")
            dt = datetime.datetime.fromisoformat(dt_str)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=datetime.timezone.utc)
            return dt.timestamp() + 631152000
        except Exception:
            return 0.0

class FFTRequest(BaseModel):
    input_file: str
    center_freq: float = 0
    zoom_center: float = 0
    zoom_bw: float = 0
    start_time: float = 0
    duration: float = 0
    window_size: int = 1024
    smoothing: int = 1

class PSDRequest(BaseModel):
    input_file: str
    center_freq: float = 0
    zoom_center: float = 0
    zoom_bw: float = 0
    start_time: float = 0
    duration: float = 0
    window_size: int = 1024
    smoothing: int = 1

class TimeDomainRequest(BaseModel):
    input_file: str
    start_time: float = 0.0
    duration: float = 0.0
    target_points: int = 10000

class PlotRequest(BaseModel):
    input_file: str
    center_freq: float = 0
    zoom_center: float = 0
    zoom_bw: float = 0
    window_size: int = 1024
    start_time: float = 0
    duration: float = 0
    smoothing: int = 1
    colormap: str = "jet"
    plot_fft: bool = True
    plot_waterfall: bool = True
    plot_time_domain: bool = False
    width: int = 1024
    height: int = 512
    theme: str = "dark"
    fill_mode: str = "gradient"
    fill_color: str = "#00FF00"
    zmin: Optional[float] = None
    zmax: Optional[float] = None

class ConvertRequest(BaseModel):
    input_file: str
    output_file: str
    format: str = "CF"
    rate: float = 1.0
    freq_mhz: float = 0.0
    timecode: str = ""

class TunerRequest(BaseModel):
    input_file: str
    output_file: str
    center_freq: float # in Hz
    bandwidth: float # in Hz
    start_time: float # in seconds
    duration: float # in seconds
    file_center: float # in Hz
    quality: str = "normal"

class UpdateRequest(BaseModel):
    timecode: str = ""
    center_freq: float = 0.0

@app.post("/api/update/{filename}")
async def update_file_metadata(filename: str, req: UpdateRequest):
    """
    Update the metadata header of a signal file.
    
    This endpoint modifies the file header (e.g., X-Midas Bluefile header) to set the 
    timecode and center frequency in place. This allows manual correction of missing or incorrect metadata 
    so that downstream processing and plotting display accurate time and frequency axis information.
    Specifically, it parses standard ISO-8601 timestamps and converts them into the precise 
    J1950 epoch format required by the native C++ DSP pipeline. Correct metadata is absolutely 
    critical for ensuring that interactive SigPlot visualizations align correctly with real-world events.
    """
    file_path = os.path.join(DATA_DIR, filename)
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="File not found")
    try:
        import dsp_plotter_py
        timecode_j1950 = parse_time_to_j1950(req.timecode)
        await asyncio.to_thread(dsp_plotter_py.DspEngine.update_header, file_path, timecode_j1950, req.center_freq)
        return {"status": "success", "timecode": timecode_j1950, "center_freq": req.center_freq}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/convert")
async def run_convert(req: ConvertRequest):
    """
    Convert a generic signal file into the canonical X-Midas Blue format.
    
    This endpoint takes an input file (such as WAV or raw binary IQ format) and uses the high-performance 
    dsp_convert tool to restructure the file into an X-Midas Blue (.prm) file. This is an essential step 
    for importing external data captures into the platform for optimized visualization and DSP. 
    The generated .prm file pairs a highly efficient binary payload with a rich metadata header, 
    allowing our core C++ plotting tools to seamlessly memory-map (mmap) the data for instantaneous 
    streaming into the web interface.
    """
    in_path = os.path.join(DATA_DIR, req.input_file)
    out_path = os.path.join(DATA_DIR, req.output_file)
    if not os.path.exists(in_path):
        raise HTTPException(status_code=404, detail="Input file not found")
        
    try:
        await asyncio.to_thread(
            dsp_wrapper.run_convert,
            in_path,
            out_path,
            req.format,
            req.rate,
            req.freq_mhz,
            False, # sigmf
            parse_time_to_j1950(req.timecode)
        )
        return {"status": "success", "output_file": req.output_file}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/tuner")
async def run_tuner(req: TunerRequest):
    """
    Digitally down-converts (DDC) and time-slices a segment of the signal into a new file.
    
    This endpoint utilizes the dsp_tuner C++ tool to isolate a specific time and frequency band 
    from the input file. The output is a new X-Midas Bluefile with a reduced, optimal sample rate 
    and an updated timecode reflecting the cropped offset. This allows for deep extraction of 
    signals of interest while perfectly preserving phase coherency.
    """
    in_path = os.path.join(DATA_DIR, req.input_file)
    out_path = os.path.join(DATA_DIR, req.output_file)
    if not os.path.exists(in_path):
        raise HTTPException(status_code=404, detail="Input file not found")
        
    try:
        await asyncio.to_thread(
            dsp_wrapper.run_tuner,
            in_path,
            out_path,
            req.center_freq,
            req.bandwidth,
            req.start_time,
            req.duration,
            req.file_center,
            True, # file_center_provided
            req.quality
        )
        return {"status": "success", "output_file": req.output_file}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/api/run/fft")
async def run_fft(req: FFTRequest):
    """
    Generate an interactive 1D FFT Spectrum.
    
    Executes the highly-optimized dsp_fft C++ tool on the target signal file to calculate the 
    power spectral density (PSD). It performs heavily vectorized FFT operations via the KFR library 
    to rapidly process massive datasets. It returns an X-Midas Type 2000 file that can be instantly 
    rendered in the browser via SigPlot, supporting full dynamic zooming, panning, and real-time 
    interaction. The output bypasses standard image formats in favor of raw spectral vectors, 
    allowing the frontend client to dictate the visual presentation dynamically.
    """
    try:
        in_path = os.path.join(DATA_DIR, req.input_file)
        if not os.path.exists(in_path):
            raise HTTPException(status_code=404, detail="Input file not found")
        
        out_id = f"fft_{uuid.uuid4().hex[:8]}.prm"
        data = await asyncio.to_thread(
            dsp_wrapper.run_fft,
            in_path,
            req.center_freq,
            req.zoom_center,
            req.zoom_bw,
            req.start_time,
            req.duration,
            req.window_size,
            req.smoothing
        )
        add_to_cache(out_id, data)
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/psd")
async def run_psd(req: PSDRequest):
    """
    Generate an interactive 2D Waterfall/Spectrogram.
    
    Executes the dsp_psd C++ tool on the target signal file to calculate a continuous time-frequency spectrogram. 
    It leverages sliding windows and advanced overlapping (Welch's method) to produce high-fidelity spectral 
    slices over time. It returns an X-Midas Type 2000 file that is natively rendered as an interactive 
    waterfall plot in the browser using SigPlot. This allows for deep inspection of signal transients, 
    bursts, and anomalies over time with lossless coordinate tracking and crosshair measurement capabilities.
    """
    try:
        in_path = os.path.join(DATA_DIR, req.input_file)
        if not os.path.exists(in_path):
            raise HTTPException(status_code=404, detail="Input file not found")
        
        out_id = f"psd_{uuid.uuid4().hex[:8]}.prm"
        data = await asyncio.to_thread(
            dsp_wrapper.run_psd,
            in_path,
            req.center_freq,
            req.zoom_center,
            req.zoom_bw,
            req.start_time,
            req.duration,
            req.window_size,
            req.smoothing
        )
        add_to_cache(out_id, data)
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/time_domain")
async def run_time_domain(req: TimeDomainRequest):
    try:
        input_path = os.path.join(DATA_DIR, req.input_file)
        if not os.path.exists(input_path):
            raise HTTPException(status_code=404, detail="Input file not found")
            
        out_id = f"td_{uuid.uuid4().hex[:8]}.prm"
        data = await asyncio.to_thread(
            dsp_wrapper.run_time_domain, 
            input_path, req.start_time, req.duration, req.target_points
        )
        add_to_cache(out_id, data)
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/plot")
async def run_plot(req: PlotRequest):
    """
    Generate a static high-definition image plot.
    
    Invokes the dsp_plotter C++ tool to create a perfectly rasterized PNG or JPEG image of the 
    requested signal's spectrum or waterfall. This endpoint bypasses the interactive SigPlot renderer 
    entirely, instead using the STB single-file image library directly within the C++ pipeline to 
    encode the visual representation. This is ideal for generating static assets for reports, 
    saving processing power when interactive capabilities are not required, and applying custom 
    pixel-perfect colormaps and theming before it ever reaches the browser.
    """
    out_format = "png" if req.plot_fft else "jpg"
    out_id = f"plot_{uuid.uuid4().hex[:8]}.{out_format}"
    in_path = os.path.join(DATA_DIR, req.input_file)
    try:
        data = await asyncio.to_thread(
            dsp_wrapper.run_plot,
            in_path,
            out_format,
            req.center_freq,
            req.zoom_center,
            req.zoom_bw,
            req.start_time,
            req.duration,
            req.window_size,
            req.smoothing,
            req.plot_fft,
            req.plot_waterfall, 
            req.plot_time_domain,
            req.colormap,
            req.width,
            req.height,
            req.theme,
            req.fill_mode,
            req.fill_color,
            req.zmin if req.zmin is not None else -1000.0,
            req.zmax if req.zmax is not None else 1000.0
        )
        add_to_cache(out_id, data)
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/data/{filename}")
def get_data(filename: str):
    """
    Retrieve binary or image data by filename.
    
    Serves the underlying output files (such as .prm interactive plots or .jpg/.png static images) 
    from the application's data directory or the high-speed in-memory cache. This endpoint acts as 
    the primary content delivery mechanism for the frontend viewers. It intelligently detects the 
    file extension to set the correct MIME type (e.g., application/octet-stream for bluefiles, 
    image/jpeg for waterfall exports) ensuring the browser handles the payload properly.
    """
    if filename in memory_cache:
        data = memory_cache[filename]
        media_type = "application/octet-stream"
        if filename.endswith(".jpg"): media_type = "image/jpeg"
        elif filename.endswith(".png"): media_type = "image/png"
        return Response(content=data, media_type=media_type)
        
    file_path = os.path.join(DATA_DIR, filename)
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="File not found")
    return FileResponse(file_path)

@app.get("/api/files")
def list_files():
    """
    List all available signal files in the data directory.
    
    Scans the configured data directory and returns a sorted array of available signal files 
    (typically .prm files). This powers the frontend's file selection dropdown, allowing users 
    to choose which signal to analyze next.
    """
    search_pattern = os.path.join(DATA_DIR, "*.prm")
    files = glob.glob(search_pattern)
    return {"files": [os.path.basename(f) for f in sorted(files)]}

@app.get("/api/info/{filename}")
def get_file_info(filename: str):
    """
    Extract technical information and metadata from a signal file.
    
    Parses the header of the specified file (supporting WAV, X-Midas Blue, etc.) to extract crucial 
    information such as the sample rate, center frequency, timecode, format type, and total duration. 
    This allows the UI to populate technical readout panels dynamically.
    """
    file_path = os.path.join(DATA_DIR, filename)
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="File not found")
    try:
        import dsp_plotter_py
        channels, sample_rate, is_wav, is_blue, format_str, timecode, center_freq = dsp_plotter_py.DspEngine(1024).get_file_info(file_path)

        file_size = os.path.getsize(file_path)
        header_size = 44 if is_wav else (512 if is_blue else 0)
        data_size = max(0, file_size - header_size)
        bps = 4
        if 'int8' in format_str: bps = 1
        elif 'int16' in format_str: bps = 2
        elif 'double' in format_str or '64' in format_str: bps = 8
        total_samples = int(data_size / (bps * channels)) if channels > 0 else 0
        duration = total_samples / sample_rate if sample_rate > 0 else 0.0

        return {
            "channels": channels,
            "sample_rate": sample_rate,
            "is_wav": is_wav,
            "is_blue": is_blue,
            "format_str": format_str,
            "timecode": timecode,
            "center_freq": center_freq,
            "file_size": file_size,
            "total_samples": total_samples,
            "duration_seconds": duration
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/upload")
def upload_file(file: UploadFile = File(...)):
    """
    Upload a new signal file to the server.
    
    Accepts a multipart file upload and saves it directly into the application's data directory. 
    It restricts uploads to supported extensions like .prm, .wav, and .raw to ensure compatibility 
    with the suite of DSP processing and conversion tools.
    """
    valid_exts = (".prm", ".wav", ".raw", ".dat", ".bin")
    if not any(file.filename.lower().endswith(ext) for ext in valid_exts):
        raise HTTPException(status_code=400, detail=f"Only {', '.join(valid_exts)} files are allowed")
    
    file_path = os.path.join(DATA_DIR, file.filename)
    try:
        with open(file_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    return {"status": "success", "filename": file.filename}

# Serve static frontend files if they exist (built via vite)
frontend_dir = os.path.join(os.path.dirname(__file__), "../frontend/dist")
if os.path.exists(frontend_dir):
    app.mount("/", StaticFiles(directory=frontend_dir, html=True), name="frontend")
