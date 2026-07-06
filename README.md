# DSP Plotter

![Example Output](waterfall.jpg)

A high-performance C++ tool for generating FFT spectrum and waterfall image plots from complex and real signals (including SigMF, X-Midas Blue, and standard WAV files). It is designed to be highly memory-efficient through streaming data via `mmap()` and fast by offloading heavy math to the SIMD-accelerated KFR framework. 

## Features

- **Blazing Fast**: Uses KFR for vectorized FFT operations and STB for rapid PNG and JPG encoding.
- **Low Memory Footprint**: Processes large signal data input efficiently via memory mapping (`mmap()`).
- **Flexible Formats**: Native auto-detection and support for SigMF (`.sigmf-meta`/`.sigmf-data`), X-Midas Blue (`.prm`/`.tmp`), and standard WAV files (Real or Complex/IQ).
- **DSP Utilities**: Includes high-performance, standalone command-line tools for Bluefile signal processing:
  - `dsp_resample`: Arbitrary interpolation/decimation using high-quality FIR polyphase filters.
  - `dsp_filter`: Real/Complex FIR filtering (lowpass, highpass, bandpass, bandstop) with accurate group delay and timecode correction.
  - `dsp_tuner`: Digital Down Converter (DDC) that automatically shifts target frequencies to baseband (0 Hz) and decimates to the optimal Nyquist bandwidth.
  - `dsp_format`: Fast conversion between real and complex signals (Padding, Hilbert, Mag, Phase, I/Q) and numeric type casting.
- **Dynamic Zooming & Channelization**: Select specific center frequencies and bandwidth limits to "zoom" into a narrow sliver of the spectrum at a very high resolution. The engine automatically mixes, FIR-filters, and decimates wideband streams on the fly to conserve memory and massively speed up processing!
- **Multiple Windowing Functions**: Hann, Hamming, Blackman-Harris (default), Flattop, and Bartlett.
- **Time/Frequency Slicing**: Process only a specific time duration instead of the entire file.
- **Highlight Overlay**: Draw semi-transparent bounding boxes directly onto the generated plots to highlight specific signals.

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

### Annotating Plots

You can draw a colored, semi-transparent highlight box over specific signals by providing the box's start time, duration, center frequency, and bandwidth. 

```bash
./dsp_plotter -i my_recording.wav --box-start-time 1.5 --box-duration 2.0 --box-center-freq 105.9 --box-bw 0.2 --box-color red
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

## DSP Command-Line Utilities

This project also provides extremely fast, standalone C++ utilities for manipulating X-Midas Bluefiles. These utilities support both real and complex data, process files out-of-core using memory mapping, and perfectly preserve/correct timecodes.

### 1. `dsp_tuner` (Digital Down Converter)
Tunes to a target center frequency (shifting it to 0 Hz baseband) and optimally decimates the signal. The output sample rate is automatically set to the target bandwidth, guaranteeing an optimal anti-aliasing lowpass filter.
```bash
# Tunes to 1.5 MHz and extracts a 500 kHz bandwidth. 
# Output will be automatically decimated to 500 kSps.
./dsp_tuner -i input.prm -o tuned.prm -c 1500000 -b 500000
```

### 2. `dsp_resample`
High-performance arbitrary sample rate converter for real and complex bluefiles.
```bash
# Resample to exactly 4.0 MHz (calculates interp/dec automatically)
./dsp_resample -i input.prm -o resampled.prm -r 4000000

# Specify exact interpolation and decimation factors manually
./dsp_resample -i input.prm -o resampled.prm -I 4 -d 3
```

### 3. `dsp_filter`
High-performance FIR filtering utility supporting lowpass, highpass, bandpass, and bandstop filters.

**Filter Types Explained:**
- **`lowpass`**: Keeps frequencies *below* a certain point and throws away the rest. Used to clean up a signal by removing high-frequency noise or static.
- **`highpass`**: Keeps frequencies *above* a certain point. Useful for removing low-frequency hums (like a 60 Hz electrical hum) or DC offsets.
- **`bandpass`**: Keeps frequencies *between* two points. Used to isolate a specific signal of interest (like tuning into a single radio station) and blocking everything else.
- **`bandstop`** (also known as a notch filter): Blocks frequencies *between* two points but lets everything else pass. Useful for surgically removing a specific piece of interference from a wideband signal.

```bash
# Lowpass filter with a 500 kHz cutoff (keeps everything from 0 to 500 kHz)
./dsp_filter -i input.prm -o filtered.prm -t lowpass --cutoff1 500000

# Bandpass filter from 100 kHz to 400 kHz using 2047 taps
./dsp_filter -i input.prm -o filtered.prm -t bandpass --cutoff1 100000 --cutoff2 400000 --taps 2047
```

### 4. `dsp_format`
High-performance formatting utility to convert between real (scalar) and complex data types, and to cast between primitive types (e.g., float to double or int16).

#### Real to Complex Conversion (`--to-complex`)
When converting a Real signal to Complex, you must specify a `--method`:
* **`hilbert`**: Applies a Hilbert transform to mathematically eliminate all negative frequencies, yielding a true Analytic Signal. The signal remains at its original positive frequency. The output sample rate matches the input sample rate.
* **`pack`**: Demodulates a passband signal centered at $F_s/4$ down to complex baseband (0 Hz) and halves the sample rate. This is ideal for quickly recovering complex IQ data from an IF-sampled real signal.
* **`pad`**: Leaves the data completely unmodified by simply zeroing out the imaginary components ($Q=0$).

#### Complex to Real Conversion (`--to-real`)
When converting a Complex signal to Real, you must specify an `--extract` method:
* **`unpack`**: Modulates a complex baseband signal up to a passband signal centered at $F_s/4$ (relative to the new sample rate). It automatically doubles the sample rate. This is the exact inverse of `--method pack`.
* **`mag`**: Extracts the magnitude ($\sqrt{I^2 + Q^2}$) of each complex frame. Useful for AM demodulation.
* **`phase`**: Extracts the phase ($\arctan(Q/I)$) of each complex frame. Useful for FM/PM demodulation.
* **`i` / `q`**: Extracts only the real ($I$) or imaginary ($Q$) components, discarding the other.

#### Examples
```bash
# Convert a real signal to an analytic complex signal using a Hilbert transform filter
./dsp_format -i real.prm -o analytic.prm --to-complex --method hilbert

# Demodulate a real passband signal (centered at Fs/4) to complex baseband (0 Hz)
./dsp_format -i if_real.prm -o baseband.prm --to-complex --method pack

# Modulate a complex baseband signal up to a real passband signal
./dsp_format -i baseband.prm -o if_real.prm --to-real --extract unpack

# Extract the magnitude from a complex signal (e.g., for AM demodulation)
./dsp_format -i complex.prm -o magnitude.prm --to-real --extract mag

# Cast a 32-bit float file (SF) to a 64-bit double file (SD)
./dsp_format -i float.prm -o double.prm --cast D
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
- **Colormaps**: The Turbo colormap was developed by Anton Mikhailov at [Google AI](https://blog.research.google/2019/08/turbo-improved-rainbow-colormap-for.html). Other colormaps (including Electric, GQRX, WebSDR) were derived from popular open-source SDR software.

## Licensing

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**. 
Because this software links against and uses the KFR DSP library (which is dual-licensed and released under GPLv2+ for open source projects), this project inherits those copyleft requirements. See the `LICENSE` file for more details.
