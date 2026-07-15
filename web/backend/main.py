import os
import uuid
import glob
import shutil
from fastapi import FastAPI, BackgroundTasks, HTTPException, UploadFile, File
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import dsp_wrapper
import time

app = FastAPI(title="DSP Web Interface")

DATA_DIR = os.getenv("DSP_DATA_DIR", "/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")

def cleanup_old_files():
    now = time.time()
    for pattern in ["fft_*.prm", "psd_*.prm", "plot_*.jpg", "plot_*.png"]:
        for f in glob.glob(os.path.join(DATA_DIR, pattern)):
            if os.path.isfile(f) and os.stat(f).st_mtime < now - 300: # 5 mins old
                try:
                    os.remove(f)
                except:
                    pass

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
    width: int = 1024
    height: int = 512

@app.post("/api/run/fft")
def run_fft(req: FFTRequest):
    cleanup_old_files()
    try:
        in_path = os.path.join(DATA_DIR, req.input_file)
        if not os.path.exists(in_path):
            raise HTTPException(status_code=404, detail="Input file not found")
        
        out_id = f"fft_{uuid.uuid4().hex[:8]}.prm"
        out_path = os.path.join(DATA_DIR, out_id)
        dsp_wrapper.dsp_fft(
            input_file=in_path,
            output_file=out_path,
            center_freq=req.center_freq,
            zoom_center=req.zoom_center,
            zoom_bw=req.zoom_bw,
            start_time=req.start_time,
            duration=req.duration,
            window_size=req.window_size,
            smoothing=req.smoothing
        )
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/psd")
def run_psd(req: PSDRequest):
    cleanup_old_files()
    try:
        in_path = os.path.join(DATA_DIR, req.input_file)
        if not os.path.exists(in_path):
            raise HTTPException(status_code=404, detail="Input file not found")
        
        out_id = f"psd_{uuid.uuid4().hex[:8]}.prm"
        out_path = os.path.join(DATA_DIR, out_id)
        dsp_wrapper.dsp_psd(
            input_file=in_path,
            output_file=out_path,
            center_freq=req.center_freq,
            zoom_center=req.zoom_center,
            zoom_bw=req.zoom_bw,
            start_time=req.start_time,
            duration=req.duration,
            window_size=req.window_size,
            smoothing=req.smoothing
        )
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/run/plot")
def run_plot(req: PlotRequest):
    cleanup_old_files()
    out_id = f"plot_{uuid.uuid4().hex[:8]}.jpg"
    out_path = os.path.join(DATA_DIR, out_id)
    in_path = os.path.join(DATA_DIR, req.input_file)
    try:
        dsp_wrapper.dsp_plotter(
            input_file=in_path,
            output_image=out_path,
            center_freq=req.center_freq,
            zoom_center=req.zoom_center,
            zoom_bw=req.zoom_bw,
            start_time=req.start_time,
            duration=req.duration,
            plot_fft=req.plot_fft,
            plot_waterfall=req.plot_waterfall,
            colormap=req.colormap,
            width=req.width,
            height=req.height,
            smoothing=req.smoothing
        )
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/data/{filename}")
def get_data(filename: str):
    file_path = os.path.join(DATA_DIR, filename)
    if not os.path.exists(file_path):
        raise HTTPException(status_code=404, detail="File not found")
    return FileResponse(file_path)

@app.get("/api/files")
def list_files():
    search_pattern = os.path.join(DATA_DIR, "*.prm")
    files = glob.glob(search_pattern)
    return {"files": [os.path.basename(f) for f in sorted(files)]}

@app.post("/api/upload")
def upload_file(file: UploadFile = File(...)):
    if not file.filename.endswith(".prm"):
        raise HTTPException(status_code=400, detail="Only .prm files are allowed")
    
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
