import os
import sys
from typing import Optional, List, Dict, Any

DSP_BIN_DIR = os.getenv("DSP_BIN_DIR", "/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")
if DSP_BIN_DIR not in sys.path:
    sys.path.append(DSP_BIN_DIR)

import os

def run_fft(input_file: str, center_freq: float, zoom_center: float, zoom_bw: float, start_time: float, duration: float, window_size: int, smoothing: int) -> bytes:
    """Runs FFT pipeline via native python bindings and returns the generated Type 1000 Bluefile bytes."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_fft_pipeline(
            input_file, center_freq, zoom_center, zoom_bw, start_time, duration, window_size, smoothing
        )
    except Exception as e:
        raise RuntimeError(f"FFT Pipeline failed: {e}")

def run_psd(input_file: str, center_freq: float, zoom_center: float, zoom_bw: float, start_time: float, duration: float, window_size: int, smoothing: int) -> bytes:
    """Runs PSD pipeline via native python bindings and returns the generated Type 2000 Bluefile bytes."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_psd_pipeline(
            input_file, center_freq, zoom_center, zoom_bw, start_time, duration, window_size, smoothing
        )
    except Exception as e:
        raise RuntimeError(f"PSD Pipeline failed: {e}")

def run_plot(input_file: str, out_format: str, center_freq: float, zoom_center: float, zoom_bw: float, start_time: float, duration: float, window_size: int, smoothing: int, plot_fft: bool, plot_waterfall: bool, colormap: str, width: int, height: int, theme: str = "dark", fill_mode: str = "gradient", fill_color: str = "#00FF00", zmin: float = -1000.0, zmax: float = 1000.0) -> bytes:
    """Runs Plot pipeline via native python bindings and returns the generated image bytes."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_plot_pipeline(
            input_file, out_format, center_freq, zoom_center, zoom_bw, start_time, duration,
            window_size, smoothing, plot_fft, plot_waterfall, colormap, width, height, theme, fill_mode, fill_color, zmin, zmax
        )
    except Exception as e:
        raise RuntimeError(f"Plot Pipeline failed: {e}")

def run_convert(input_file: str, output_file: str, format: str = "", rate: float = 1.0, freq_mhz: float = 0.0, sigmf: bool = False, timecode: float = 0.0):
    """Runs conversion pipeline via native python bindings."""
    try:
        import dsp_plotter_py
        dsp_plotter_py.run_convert(input_file, output_file, format, rate, freq_mhz, sigmf, timecode)
    except Exception as e:
        raise RuntimeError(f"Convert Pipeline failed: {e}")
