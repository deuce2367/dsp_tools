import os
import uuid
import glob
import shutil
from fastapi import FastAPI, BackgroundTasks, HTTPException, UploadFile, File
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
import dsp_wrapper
import time
import asyncio
import collections

app = FastAPI(title="DSP Web Interface")

DATA_DIR = os.getenv("DSP_DATA_DIR", "/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")

memory_cache = collections.OrderedDict()

def add_to_cache(key: str, data: bytes):
    memory_cache[key] = data
    # Evict oldest if we have more than 50 cached files
    while len(memory_cache) > 50:
        memory_cache.popitem(last=False)

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
async def run_fft(req: FFTRequest):
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

@app.post("/api/run/plot")
async def run_plot(req: PlotRequest):
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
            req.colormap,
            req.width,
            req.height
        )
        add_to_cache(out_id, data)
        return {"status": "success", "output_file": out_id}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/data/{filename}")
def get_data(filename: str):
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
