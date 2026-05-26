import sys
import os

# Add build directory to path to find the dsp_plotter_py.so module
build_dir = os.path.join(os.path.dirname(__file__), 'build')
sys.path.append(build_dir)

import dsp_plotter_py

def main():
    print("========================================")
    print("DSP Plotter Python Bindings Example")
    print("========================================")
    
    # 1. Initialize the DSP Engine
    # The argument is the default FFT window size (will be overridden if the config requests a different size)
    engine = dsp_plotter_py.DspEngine(1024)
    
    test_file = "example.prm"
    if not os.path.exists(test_file):
        print(f"Error: {test_file} not found. Please ensure it exists in the current directory.")
        return
        
    # 2. Extract File Info
    # Returns a tuple: (channels, sample_rate, is_wav, is_blue, format_str, timecode)
    info = engine.get_file_info(test_file)
    print(f"[{test_file}] Detected format: {info[4]}, Channels: {info[0]}, Rate: {info[1]} Hz")
    
    # 3. Configure the Processing Job
    config = dsp_plotter_py.StreamConfig()
    config.filename = test_file
    
    # We must provide basic parameters. In a real script, you might extract these
    # from a SigMF file or the user, but for now we'll set some defaults based on the header.
    config.sample_rate = info[1] if info[1] > 1.0 else 1000000.0  # 1 MHz default
    config.is_wav = info[2]
    config.is_blue = info[3]
    
    # If it's a raw file, we must tell the engine if it's complex or real
    if not config.is_wav and not config.is_blue:
        config.data_type = "complex" if info[0] == 2 else info[4]
    else:
        config.data_type = info[4]
    
    # Configure output image dimensions (determines how many FFTs are calculated)
    out_width = 800
    out_height = 600
    config.output_width = out_width
    config.output_height = out_height
    
    # Define zoom parameters (optional)
    center_freq_mhz = 0.0
    bandwidth_mhz = config.sample_rate / 1e6
    config.zoom_center = center_freq_mhz
    config.zoom_bw = bandwidth_mhz
    
    # 4. Execute the C++ DSP Math Engine
    print("\nProcessing file via C++ DSP engine (blazing fast!)...")
    result = engine.process_file_streaming(config)
    
    # The C++ std::vectors are instantly accessible as Python lists!
    spectrogram = result.spectrogram
    print(f"-> Output Spectrogram Shape: {len(spectrogram)} rows x {len(spectrogram[0])} bins")
    
    # 5. Generate Visualizations via C++ PlotGenerator
    print("\nRendering plots via C++ backend...")
    
    # To prevent garbled overlapping text, dynamically calculate the number of axis ticks based on image width
    # This mimics the smart-scaling logic from the main C++ CLI
    num_x_ticks = max(1, out_width // 256)
    num_y_ticks = max(1, out_height // 64)
    
    min_db = -100.0
    max_db = 0.0
    
    # ---------------------------------------------
    # Example A: Render an FFT Plot
    # ---------------------------------------------
    # generate_fast_fft_plot needs an array of frequency bins, though they are purely for the x-axis mapping
    dummy_freqs = [i for i in range(len(result.avg_fft))]
    
    fft_filename = "example_python_fft.png"
    dsp_plotter_py.PlotGenerator.generate_fast_fft_plot(
        frequency_bins=dummy_freqs,
        magnitude_db=result.avg_fft,          # Use the average FFT array for a smooth plot
        output_filename=fft_filename,
        out_width=out_width,
        out_height=out_height,
        min_db=min_db,
        max_db=max_db,
        center_freq_mhz=center_freq_mhz,
        bandwidth_mhz=bandwidth_mhz,
        draw_grid=True,
        draw_labels=True,
        out_format="png",
        num_x_ticks=num_x_ticks,              # Prevent axis garbling!
        num_y_ticks=num_y_ticks,
        title="Python FFT Plot (Average)",
        jpeg_quality=90,
        png_compression=8,
        colormap_name="turbo"                 # Give it a nice color!
    )
    print(f"-> Saved {fft_filename}")
    
    # ---------------------------------------------
    # Example B: Render a Waterfall (Spectrogram)
    # ---------------------------------------------
    waterfall_filename = "example_python_waterfall.png"
    
    # For the waterfall, we provide a starting ISO time and total duration 
    # to populate the Y-axis time labels.
    start_time_iso = "2026-05-25T12:00:00Z"
    total_duration_sec = 5.0  # Dummy duration
    
    dsp_plotter_py.PlotGenerator.generate_fast_waterfall(
        spectrogram_db=spectrogram,           # The massive 2D array
        output_filename=waterfall_filename,
        out_width=out_width,
        out_height=out_height,
        colormap="turbo",                     # Valid options: turbo, magma, inferno, plasma, viridis, jet, etc.
        min_db=min_db,
        max_db=max_db,
        center_freq_mhz=center_freq_mhz,
        bandwidth_mhz=bandwidth_mhz,
        start_time_iso=start_time_iso,
        total_duration_sec=total_duration_sec,
        draw_grid=True,
        draw_labels=True,
        out_format="png",
        num_x_ticks=num_x_ticks,              # Prevent axis garbling!
        num_y_ticks=num_y_ticks,
        title="Python Waterfall Plot",
        jpeg_quality=90,
        png_compression=8
    )
    print(f"-> Saved {waterfall_filename}")
    print("\nDone! Check out the generated images.")

if __name__ == "__main__":
    main()
