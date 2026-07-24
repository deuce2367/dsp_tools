import os
import sys
from typing import Optional, List, Dict, Any

DSP_BIN_DIR = os.getenv("DSP_BIN_DIR", "/home/apsmith/.gemini/antigravity/scratch/dsp_tools/build")
if DSP_BIN_DIR not in sys.path:
    sys.path.append(DSP_BIN_DIR)

import os

def run_fft(input_file: str, center_freq: float, zoom_center: float, zoom_bw: float, start_time: float, duration: float, window_size: int, smoothing: int, window_type: str = "blackman-harris", stride_ratio: float = 0.0) -> tuple:
    """Runs FFT pipeline via native python bindings and returns the generated Type 1000 Bluefile bytes and detected min/max dB."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_fft_pipeline(
            input_file, center_freq, zoom_center, zoom_bw, start_time, duration, window_size, smoothing, window_type, stride_ratio
        )
    except Exception as e:
        raise RuntimeError(f"FFT Pipeline failed: {e}")

def run_psd(input_file: str, center_freq: float, zoom_center: float, zoom_bw: float, start_time: float, duration: float, window_size: int, smoothing: int, window_type: str = "blackman-harris", stride_ratio: float = 0.0) -> tuple:
    """Runs PSD pipeline via native python bindings and returns the generated Type 2000 Bluefile bytes and detected min/max dB."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_psd_pipeline(
            input_file, center_freq, zoom_center, zoom_bw, start_time, duration, window_size, smoothing, window_type, stride_ratio
        )
    except Exception as e:
        raise RuntimeError(f"PSD Pipeline failed: {e}")

def run_plot(input_file: str, out_format: str, center_freq: float, zoom_center: float, zoom_bw: float, start_time: float, duration: float, window_size: int, smoothing: int, window_type: str, stride_ratio: float, plot_fft: bool, plot_waterfall: bool, plot_time_domain: bool, time_domain_mode: str, plot_constellation: bool, colormap: str, width: int, height: int, theme: str = "dark", fill_mode: str = "gradient", fill_color: str = "#00FF00", zmin: float = -1000.0, zmax: float = 1000.0) -> bytes:
    """Runs Plot pipeline via native python bindings and returns the generated image bytes."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_plot_pipeline(
            input_file, out_format, center_freq, zoom_center, zoom_bw, start_time, duration,
            window_size, smoothing, window_type, stride_ratio, plot_fft, plot_waterfall, plot_time_domain, time_domain_mode, plot_constellation, colormap, width, height, theme, fill_mode, fill_color, zmin, zmax
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

def run_tuner(input_file: str, output_file: str, center: float, bandwidth: float, start_time: float = 0.0, duration: float = 0.0, file_center: float = 0.0, file_center_provided: bool = False, quality_str: str = "normal", oversample_factor: int = 1):
    """Runs digital down-conversion pipeline via native python bindings."""
    try:
        import dsp_plotter_py
        dsp_plotter_py.run_tuner_pipeline(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality_str, oversample_factor)
    except Exception as e:
        raise RuntimeError(f"Tuner Pipeline failed: {e}")

def run_time_domain(input_file: str, start_time: float = 0.0, duration: float = 0.0, target_points: int = 10000, mode: str = "complex") -> bytes:
    """Runs time domain envelope pipeline via native python bindings and returns bytes."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_time_domain(input_file, start_time, duration, target_points, mode)
    except Exception as e:
        raise RuntimeError(f"Time Domain Pipeline failed: {e}")

def run_constellation_data(input_file: str, start_time: float = 0.0, duration: float = 0.0, max_points: int = 100000) -> bytes:
    """Extracts raw IQ pairs for a constellation plot via native python bindings."""
    try:
        import dsp_plotter_py
        return dsp_plotter_py.run_constellation_data(input_file, start_time, duration, max_points)
    except Exception as e:
        raise RuntimeError(f"Constellation Pipeline failed: {e}")

def run_resample(input_file: str, output_file: str, new_rate: float, quality: str = "normal"):
    """Runs resampling pipeline via native python bindings."""
    try:
        import dsp_plotter_py
        dsp_plotter_py.run_resample(input_file, output_file, new_rate, quality)
    except Exception as e:
        raise RuntimeError(f"Resample Pipeline failed: {e}")

def run_filter(input_file: str, output_file: str, filter_type: str, cutoff1: float, cutoff2: float = 0.0, taps: int = 1023, center_freq: float = 0.0):
    """Runs filtering pipeline via native python bindings."""
    try:
        import dsp_plotter_py
        dsp_plotter_py.run_filter(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq)
    except Exception as e:
        raise RuntimeError(f"Filter Pipeline failed: {e}")

def run_whiten(input_file: str, output_file: str, fft_size: int = 4096, alpha: float = 0.99, blank_threshold: float = 10.0, blank_window: int = 1024, output_gain: float = 0.0, strength: float = 0.5, mode: str = "compress", excess_leak: float = -100.0):
    """Runs whitening / pulse blanker pipeline via native python bindings."""
    try:
        import dsp_plotter_py
        dsp_plotter_py.run_whiten(input_file, output_file, fft_size, alpha, blank_threshold, blank_window, output_gain, strength, mode, excess_leak)
    except Exception as e:
        raise RuntimeError(f"Whiten Pipeline failed: {e}")
