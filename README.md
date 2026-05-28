# DSP Plotter

![Example Output](waterfall.jpg)

A high-performance C++ tool for generating FFT spectrum and waterfall plots from complex and real signals (including SigMF, X-Midas Blue, and standard WAV files). It is designed to be highly memory-efficient through streaming data via `mmap()` and fast by offloading heavy math to the SIMD-accelerated KFR framework. 

## Features

- **Blazing Fast**: Uses KFR for vectorized FFT operations and STB for rapid PNG encoding.
- **Low Memory Footprint**: Processes large signal data input efficiently via memory mapping (`mmap()`).
- **Flexible Formats**: Native auto-detection and support for SigMF (`.sigmf-meta`/`.sigmf-data`), X-Midas Blue (`.prm`/`.tmp`), and standard WAV files (Real or Complex/IQ).
- **Dynamic Zooming**: Select specific center frequencies and bandwidth limits to "zoom" into a narrow sliver of the spectrum at a very high resolution.
- **Multiple Windowing Functions**: Hann, Hamming, Blackman-Harris (default), Flattop, and Bartlett.
- **Time/Frequency Slicing**: Process only a specific time duration instead of the entire file.

## Build Instructions

This project is built using CMake and requires a modern C++ compiler (C++17 or newer).

### Dependencies
All core dependencies (KFR, spdlog, CLI11, stb) are automatically fetched and built using CMake's `FetchContent`. No manual system installations of these libraries are required!

### Compiling

1. Clone or download this repository.
2. Create a build directory:
   ```bash
   mkdir build && cd build
   ```
3. Run CMake and Make:
   ```bash
   cmake ..
   make -j$(nproc)
   ```

After compilation, the binary will be located at `build/dsp_plotter`.

## Usage & Examples

### Basic Usage

To generate a fast waterfall plot of an entire SDR recording:
```bash
./dsp_plotter -i my_recording.wav
```
This will automatically determine if the signal is Real or Complex, and plot the entire bandwidth.

### Generating Both FFT and Waterfall Plots

Use the `--plot-fft` and `--plot-waterfall` flags to explicitly request both image types:
```bash
./dsp_plotter -i my_recording.wav --plot-fft --plot-waterfall
```

### Zooming into a Signal

If you have a 3.0 MHz wide recording centered at 105.1 MHz, but you only want to plot a 0.4 MHz sliver centered exactly at 105.9 MHz:

```bash
./dsp_plotter -i sdrplay_105.1MHz.wav --center-freq 105.1 --zoom-center 105.9 --zoom-bw 0.4
```
*Note: `--zoom-center` takes an absolute frequency.* 

Alternatively, if you prefer to use relative offsets (e.g. +0.8 MHz from the center):
```bash
./dsp_plotter -i sdrplay_105.1MHz.wav --center-freq 105.1 --zoom-offset 0.8 --zoom-bw 0.4
```

### Changing Resolution and Quality

You can adjust the output PNG dimensions. The tool will automatically calculate the best FFT size needed to map your requested frequency bandwidth directly to your chosen pixel width!

```bash
./dsp_plotter -i my_recording.wav --width 1920 --height 1080
```

To smooth out the plot, you can adjust the number of overlapping FFTs that are averaged per vertical row (default is 4):
```bash
./dsp_plotter -i my_recording.wav --averaging 8
```

### Changing Fonts and Appearance

The plotter is 100% self-contained and bundles a high-quality embedded TrueType font (DejaVu Sans Mono) directly into the executable. This guarantees mathematically perfect, scalable axis labels on any system or OS without relying on external system fonts!

However, you can still override the font choice by specifying a custom `.ttf` file:
```bash
./dsp_plotter -i my_recording.wav --font /path/to/custom_font.ttf
```

## Acknowledgements & Third-Party Code

This project relies on several fantastic open-source projects and code snippets:

- **KFR** (https://github.com/kfrlib/kfr): A fast, modern C++ DSP framework (dual-licensed, used under GPLv2+).
- **stb_image_write.h** & **stb_truetype.h** (https://github.com/nothings/stb): Public domain single-file libraries by Sean Barrett used for rapid PNG/JPEG encoding and TrueType vector font rasterization.
- **CLI11** (https://github.com/CLIUtils/CLI11): Command line parser for C++11 and beyond (BSD 3-Clause).
- **spdlog** (https://github.com/gabime/spdlog): Fast C++ logging library (MIT License).
- **nlohmann_json** (https://github.com/nlohmann/json): JSON for Modern C++ (MIT License).
- **DejaVu Sans Mono**: The embedded default font used for high-quality axis labels (Bitstream Vera License).
- **pybind11** (https://github.com/pybind/pybind11): Seamless operability between C++11 and Python (BSD 3-Clause).
- **font8x8** (https://github.com/dhepper/font8x8): Basic 8x8 font rendering map by Daniel Hepper (Public Domain/MIT License).
- **Colormaps**: The Turbo colormap was developed by Anton Mikhailov at [Google AI](https://blog.research.google/2019/08/turbo-improved-rainbow-colormap-for.html). Other colormaps (including Electric, GQRX, WebSDR) were derived from popular open-source SDR software.

## Licensing

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. 
Because this software links against and uses the KFR DSP library (which is dual-licensed and released under GPLv2+ for open source projects), this project inherits those copyleft requirements. See the `LICENSE` file for more details.
