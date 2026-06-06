import sys
import os
import argparse
import time

# Add build directory to path to find the dsp_plotter_py.so module
build_dir = os.path.join(os.path.dirname(__file__), 'build')
sys.path.append(build_dir)

try:
    import dsp_plotter_py
except ImportError:
    print("Error: Could not import dsp_plotter_py. Make sure you have compiled the python bindings.")
    sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="DSP Plotter Python Bindings Wrapper")
    parser.add_argument("-i", "--input", required=True, help="Input binary file")
    parser.add_argument("-f", "--format", choices=["real", "complex"], help="Data format: real or complex")
    parser.add_argument("--data-type", choices=["float32", "float64", "int16", "int8"], help="Input data type")
    
    parser.add_argument("--plot-fft", action="store_true", help="Generate a 1D FFT plot")
    parser.add_argument("--plot-waterfall", action="store_true", help="Generate a 2D PSD Waterfall plot")
    
    parser.add_argument("-o", "--output", help="Output filename")
    parser.add_argument("--out-format", choices=["png", "jpg"], default="jpg", help="Output image format (default: jpg)")
    parser.add_argument("--jpeg-quality", type=int, default=90, help="JPEG quality 1-100 (default: 90)")
    parser.add_argument("--png-compression", type=int, default=8, help="PNG compression 0-9 (default: 8)")
    
    parser.add_argument("-c", "--colormap", default="gqrx", choices=["electric", "gqrx", "websdr", "pablo", "frog", "jet", "turbo", "grape"], help="Colormap for waterfall (default: gqrx)")
    
    parser.add_argument("--width", type=int, default=512, help="Output image width")
    parser.add_argument("--height", type=int, default=512, help="Output image height")
    
    parser.add_argument("--min-db", type=float, default=1.0, help="Minimum dB level (default: 1.0 for auto-leveling)")
    parser.add_argument("--max-db", type=float, default=1.0, help="Maximum dB level (default: 1.0 for auto-leveling)")
    
    parser.add_argument("--center-freq", type=float, default=0.0, help="Center frequency in MHz")
    parser.add_argument("--bandwidth", type=float, default=0.0, help="Bandwidth in MHz")
    parser.add_argument("--zoom-center", type=float, default=0.0, help="Zoom center frequency in MHz")
    parser.add_argument("--zoom-bw", type=float, default=0.0, help="Zoom bandwidth in MHz")
    
    parser.add_argument("--averaging", type=int, default=4, help="Number of overlapping FFTs to average per row (default: 4)")
    parser.add_argument("--stride-ratio", type=float, default=0.0, help="Time stride ratio relative to FFT window. E.g. 0.5 is 50% overlap. (default: auto-fit to height)")
    
    args = parser.parse_args()
    
    if not args.plot_fft and not args.plot_waterfall:
        print("Neither --plot-fft nor --plot-waterfall specified. Defaulting to waterfall.")
        args.plot_waterfall = True

    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found.")
        sys.exit(1)
        
    engine = dsp_plotter_py.DspEngine(1024)
    info = engine.get_file_info(args.input)
    # info: (channels, sample_rate, is_wav, is_blue, format_str, timecode)
    channels = info[0]
    sample_rate = info[1]
    is_wav = info[2]
    is_blue = info[3]
    format_str = info[4]
    timecode = info[5]
    
    detected_format = "complex" if channels == 2 else "real"
    data_fmt = args.format if args.format else detected_format
    dtype = args.data_type if args.data_type else format_str
    
    print("========================================")
    print("DSP Plotter Python Engine")
    print("========================================")
    print(f"[{args.input}] Detected format: {data_fmt} ({dtype}), Channels: {channels}, Rate: {sample_rate} Hz")

    config = dsp_plotter_py.StreamConfig()
    config.filename = args.input
    config.is_wav = is_wav
    config.is_blue = is_blue
    config.data_type = dtype
    if not is_wav and not is_blue and data_fmt == "complex":
        config.data_type = "complex"
        
    config.sample_rate = sample_rate
    config.center_freq = args.center_freq
    config.zoom_center = args.zoom_center
    config.zoom_bw = args.zoom_bw
    
    if args.stride_ratio > 0.0:
        config.step_size = max(1, int(config.window_size * args.stride_ratio))
    
    config.output_width = args.width
    config.output_height = args.height
    config.time_smoothing = args.averaging

    print("\nProcessing file via C++ DSP engine...")
    t0 = time.time()
    result = engine.process_file_streaming(config)
    t1 = time.time()
    
    spectrogram = result.spectrogram
    print(f"-> DSP processing took {t1 - t0:.5f} seconds")
    print(f"-> Output Spectrogram Shape: {len(spectrogram)} rows x {len(spectrogram[0])} bins")
    
    # Auto-leveling
    actual_min_db = 0.0
    actual_max_db = -1000.0
    sum_db = 0.0
    count_db = 0
    if spectrogram:
        for row in spectrogram:
            for val in row:
                if val < actual_min_db: actual_min_db = val
                if val > actual_max_db: actual_max_db = val
                sum_db += val
                count_db += 1
                
    mean_db = sum_db / count_db if count_db > 0 else -100.0
    final_min_db = (mean_db - 15.0) if args.min_db == 1.0 else args.min_db
    final_max_db = actual_max_db if args.max_db == 1.0 else args.max_db
    
    # Labels
    z_center = result.actual_zoom_center
    fs = result.actual_zoom_bw
    
    out_base = args.output if args.output else os.path.splitext(os.path.basename(args.input))[0]
    
    num_x_ticks = max(1, args.width // 100)
    num_y_ticks = max(1, args.height // 60)
    
    if args.plot_waterfall:
        wfile = out_base if args.output else f"{out_base}_waterfall.{args.out_format}"
        
        tot_dur = 0.0
        if sample_rate > 0:
            tot_dur = len(spectrogram) * result.actual_step_size / sample_rate
            
        print(f"Generating Waterfall {wfile} (Range: {final_min_db:.1f} to {final_max_db:.1f} dB)...")
        dsp_plotter_py.PlotGenerator.generate_fast_waterfall(
            spectrogram_db=spectrogram,
            output_filename=wfile,
            out_width=args.width,
            out_height=args.height,
            colormap=args.colormap,
            min_db=final_min_db,
            max_db=final_max_db,
            center_freq_mhz=z_center,
            bandwidth_mhz=fs,
            start_time_iso=f"{result.original_start_time:.2f}s",
            total_duration_sec=tot_dur,
            out_format=args.out_format,
            num_x_ticks=num_x_ticks,
            num_y_ticks=num_y_ticks,
            title=os.path.basename(args.input),
            jpeg_quality=args.jpeg_quality,
            png_compression=args.png_compression
        )
        print(f"-> Saved {wfile}")

    if args.plot_fft and spectrogram:
        ffile = out_base if args.output else f"{out_base}_fft.{args.out_format}"
        dummy_freqs = [0] * len(result.avg_fft) # Freqs computed in C++ wrapper
        
        print(f"Generating FFT {ffile} (Range: {final_min_db:.1f} to {final_max_db:.1f} dB)...")
        dsp_plotter_py.PlotGenerator.generate_fast_fft_plot(
            frequency_bins=dummy_freqs,
            magnitude_db=result.avg_fft,
            output_filename=ffile,
            out_width=args.width,
            out_height=args.height,
            min_db=final_min_db,
            max_db=final_max_db,
            center_freq_mhz=z_center,
            bandwidth_mhz=fs,
            out_format=args.out_format,
            num_x_ticks=num_x_ticks,
            num_y_ticks=num_y_ticks,
            colormap_name=args.colormap,
            title=os.path.basename(args.input),
            jpeg_quality=args.jpeg_quality,
            png_compression=args.png_compression
        )
        print(f"-> Saved {ffile}")

if __name__ == "__main__":
    main()
