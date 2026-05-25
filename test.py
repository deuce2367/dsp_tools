import sys
import os

# Add build directory to path to find dsp_plotter_py.so
build_dir = os.path.join(os.path.dirname(__file__), 'build')
sys.path.append(build_dir)

import dsp_plotter_py

def main():
    print("Testing DSP Plotter Python Bindings...")
    
    # Initialize Engine with default FFT size (e.g. 1024)
    engine = dsp_plotter_py.DspEngine(1024)
    
    # Try reading file info
    test_file = "datetime.prm"
    if os.path.exists(test_file):
        info = engine.get_file_info(test_file)
        print(f"File Info for {test_file}: {info}")
        
        # Configure the DSP engine
        config = dsp_plotter_py.StreamConfig()
        config.filename = test_file
        config.sample_rate = 1.0 # default dummy rate
        config.data_type = info[4] # format_str
        config.output_width = 512
        config.output_height = 512
        
        # Process the file
        print("Processing file via C++ engine...")
        result = engine.process_file_streaming(config)
        
        # Examine results
        spec = result.spectrogram
        print(f"Success! Spectrogram shape: {len(spec)} rows x {len(spec[0])} bins")
        print(f"Avg FFT length: {len(result.avg_fft)}")
        
        # Try generating a plot
        print("Rendering FFT Plot from Python...")
        
        # Generate dummy frequency bins for the plot
        freqs = [i for i in range(len(result.first_frame_mag))]
        
        dsp_plotter_py.PlotGenerator.generate_fast_fft_plot(
            freqs, result.first_frame_mag, "test_python_fft.png", 
            512, 512, -100.0, 0.0, 0.0, 1.0
        )
        print("Saved test_python_fft.png")
    else:
        print(f"Warning: {test_file} not found, skipping deep tests.")

if __name__ == "__main__":
    main()
