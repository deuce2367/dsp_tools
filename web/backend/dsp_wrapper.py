import subprocess
import os
from typing import Optional, List, Dict, Any

DSP_BIN_DIR = os.getenv("DSP_BIN_DIR", "/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")

def _run_tool(tool_name: str, args: List[str]) -> str:
    bin_path = os.path.join(DSP_BIN_DIR, tool_name)
    if not os.path.exists(bin_path):
        # Fallback to local dir for testing
        bin_path = f"./{tool_name}"
    
    cmd = [bin_path] + args
    print(f"Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"DSP Tool {tool_name} failed:\n{result.stderr}")
    return result.stdout

def dsp_filter(input_file: str, output_file: str, filter_type: str, cutoff1: float, cutoff2: float = 0.0, taps: int = 1023):
    args = ["-i", input_file, "-o", output_file, "-t", filter_type, "--cutoff1", str(cutoff1)]
    if filter_type in ["bandpass", "bandstop"]:
        args.extend(["--cutoff2", str(cutoff2)])
    args.extend(["--taps", str(taps)])
    return _run_tool("dsp_filter", args)

def dsp_whitener(input_file: str, output_file: str, mode: str = "compress", strength: float = 0.5, 
                 excess_leak: float = -100.0, alpha: float = 0.99, blank_threshold: float = 10.0, 
                 blank_window: int = 1024, fft_size: int = 4096):
    args = ["-i", input_file, "-o", output_file, "--mode", mode, "--strength", str(strength),
            "--excess_leak", str(excess_leak), "--alpha", str(alpha), "--blank-threshold", str(blank_threshold),
            "--blank-window", str(blank_window), "--fft-size", str(fft_size)]
    return _run_tool("dsp_whitener", args)

def dsp_tuner(input_file: str, output_file: str, center_freq: float, bandwidth: float):
    args = ["-i", input_file, "-o", output_file, "-c", str(center_freq), "-b", str(bandwidth)]
    return _run_tool("dsp_tuner", args)

def dsp_resample(input_file: str, output_file: str, rate: float):
    args = ["-i", input_file, "-o", output_file, "-r", str(rate)]
    return _run_tool("dsp_resample", args)

def dsp_format(input_file: str, output_file: str, to_complex: bool = False, to_real: bool = False, 
               method: str = "pack", extract: str = "mag", cast: str = ""):
    args = ["-i", input_file, "-o", output_file]
    if to_complex:
        args.append("--to-complex")
        args.extend(["--method", method])
    if to_real:
        args.append("--to-real")
        args.extend(["--extract", extract])
    if cast:
        args.extend(["--cast", cast])
    return _run_tool("dsp_format", args)

def dsp_fft(input_file: str, output_file: str, center_freq: float = 0, zoom_center: float = 0, zoom_bw: float = 0, start_time: float = 0, duration: float = 0, window_size: int = 1024, smoothing: int = 1):
    args = ["-i", input_file, "-o", output_file, "-c", str(center_freq), 
            "--zoom-center", str(zoom_center), "--zoom-bw", str(zoom_bw), 
            "-s", str(start_time), "-d", str(duration), "-w", str(window_size), "--smoothing", str(smoothing)]
    return _run_tool("dsp_fft", args)

def dsp_psd(input_file: str, output_file: str, center_freq: float = 0, zoom_center: float = 0, zoom_bw: float = 0, start_time: float = 0, duration: float = 0, window_size: int = 1024, smoothing: int = 1):
    args = ["-i", input_file, "-o", output_file, "-c", str(center_freq), 
            "--zoom-center", str(zoom_center), "--zoom-bw", str(zoom_bw), 
            "-s", str(start_time), "-d", str(duration), "-w", str(window_size), "--smoothing", str(smoothing), "--step-size", "0"]
    return _run_tool("dsp_psd", args)

def dsp_plotter(input_file: str, output_image: str, center_freq: float = 0, zoom_center: float = 0, 
                zoom_bw: float = 0, start_time: float = 0, duration: float = 0, 
                plot_fft: bool = True, plot_waterfall: bool = True, colormap: str = "jet",
                width: int = 512, height: int = 512, smoothing: int = 1):
    end_time = start_time + duration if duration > 0 else 0
    args = ["-i", input_file, "-o", output_image, "--center-freq", str(center_freq), 
            "--zoom-center", str(zoom_center), "--zoom-bw", str(zoom_bw), 
            "--start-time", str(start_time), "--end-time", str(end_time), 
            "--colormap", colormap, "--width", str(width), "--height", str(height),
            "--averaging", str(smoothing)]
    if plot_fft:
        args.append("--plot-fft")
    if plot_waterfall:
        args.append("--plot-waterfall")
    return _run_tool("dsp_plotter", args)
