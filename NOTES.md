# DSP Tools: Theoretical Background & Motivation Notes

This document provides background, overview, and theoretical motivation for each tool in the `dsp_tools` suite. It is intended to help users understand *why* these specific DSP operations are necessary and *when* to use them in a real-world processing pipeline.

---

## 1. `dsp_whitener` (Hybrid Interference Reducer)

### Background & Motivation
In real-world RF environments, signals of interest are rarely pristine. They are often buried under or immediately adjacent to massive sources of interference, such as continuous-wave (CW) carriers, local oscillator leakage, FM broadcast stations, or sweeping radar pulses. A standard detector or demodulator will often be captured by the loudest signal (the interference), rendering the target signal undecodable. 

The `dsp_whitener` employs a two-stage process to eliminate this interference: a time-domain pulse blanker (for radar) and a frequency-domain AGC (for CW/narrowband interference).

### Whitener vs. Overwhitener (`--mode compress` vs `--mode griffiths`)
The frequency-domain engine supports two mathematically distinct algorithms depending on the end goal:

**Compress Mode (`--mode compress`) = Partial Whitener**
*   **Purpose:** Visual analysis and dynamic range compression.
*   **How it works:** It acts as a dynamic range compressor. It gently balances the spectrum based on magnitude, making the noise floor more uniform while intentionally leaving the relative peaks of signals intact so they can still be seen or analyzed. 

**Leaky Griffiths Mode (`--mode griffiths`) = Overwhitener**
*   **Purpose:** Signal pre-conditioning for automated detection. It is almost exclusively used to prepare data for algorithms (like energy detectors or machine learning), rather than for human eyes.
*   **How it works:** It acts as a strict, aggressive mathematical normalizer (an Adaptive Interference Canceler). By using the power spectrum and a hard mathematical leak limit (`--excess_leak`), it forces *everything* above the thermal noise floor to be completely flattened out. It gets the name "overwhitener" because the interference cancellation is so absolute that it often completely obliterates the presence of the original signal, leaving nothing but statistically pure "white" noise behind.
*   **Why it's powerful:**
    1. **Constant False Alarm Rate (CFAR):** Automated energy detectors need a perfectly flat, predictable noise floor. If there is a massive CW spike in the spectrum, a standard detector will constantly trigger on it as a "false alarm." An overwhitener mathematically guarantees that the background spectrum is perfectly flat, allowing automated detectors to function reliably.
    2. **Exposing Transients and Hoppers:** Because an overwhitener uses a rolling average (controlled by the `exp_decay_constant`), it is designed to crush *persistent* or continuous signals. If a brand new, fast, or frequency-hopping signal suddenly appears, the overwhitener's memory hasn't adapted to it yet. That brief transient signal will pop up vividly above the perfectly flat noise floor, making it incredibly easy to detect!
    3. **Preventing Demodulator Capture:** By overwhitening the spectrum first, you effectively delete a dominant interferer, allowing a demodulator's AGC/PLL to lock onto the weaker, underlying signal.

---

## 2. `dsp_plotter` (High-Performance Spectral Plotter)

### Background & Motivation
Before you can effectively process an unknown RF recording, you must understand what is inside it. Blindly applying filters or demodulators to a gigabyte-sized binary file is an exercise in frustration. 

The `dsp_plotter` is the "microscope" of the DSP suite. It calculates the Short-Time Fourier Transform (STFT) of the raw IQ or real data and maps the time/frequency/magnitude dimensions to the Y/X/Color axes of a JPEG or PNG image. 

By offloading the heavy mathematics to SIMD-accelerated frameworks and natively memory-mapping large files, it can render gigabytes of SDR recordings into human-readable visual summaries in fractions of a second, allowing analysts to instantly spot target signals, fading, interference, and temporal patterns.

---

## 3. `dsp_filter` (FIR Filtering Utility)

### Background & Motivation
The most fundamental operation in signal processing is isolating a signal of interest from out-of-band noise and adjacent channel interference. 

The `dsp_filter` is a high-performance Finite Impulse Response (FIR) filtering engine. FIR filters are chosen over Infinite Impulse Response (IIR) filters in software DSP because they offer *perfect linear phase*. This is critical for digital communications: if a filter does not have linear phase, it will subject different frequencies of the signal to different time delays (group delay distortion). This distortion will "smear" the symbols of a digital modulation scheme (like QPSK or QAM) into each other, resulting in Inter-Symbol Interference (ISI) and preventing successful demodulation. 

By applying a strict FIR bandpass or lowpass filter, the noise floor power outside the channel is drastically reduced, immediately improving the Signal-to-Noise Ratio (SNR) before demodulation.

---

## 4. `dsp_tuner` (Digital Down Converter)

### Background & Motivation
Software Defined Radios (SDRs) often record massive swaths of bandwidth (e.g., 50 MHz wide) to capture everything happening in a given band. However, the specific signal you care about might only be 25 kHz wide, sitting offset at 10.5 MHz away from the center frequency. 

It is incredibly inefficient to feed a 50 MSps (Mega-Samples per second) file into a demodulator just to extract a 25 kHz signal. 

The `dsp_tuner` acts as a Digital Down Converter (DDC). It performs three mathematical steps simultaneously:
1. **Mixing:** It digitally multiplies the signal by a complex sinusoid to shift the target frequency down to 0 Hz (Baseband).
2. **Filtering:** It applies a strict lowpass anti-aliasing filter tailored specifically to the target bandwidth.
3. **Decimation:** It dramatically reduces the sample rate (e.g., throwing away 99% of the samples). 

The result is a tiny, highly concentrated file containing *only* the signal of interest at the lowest mathematically viable sample rate, saving vast amounts of disk space and CPU cycles in downstream processing.

---

## 5. `dsp_resample` (Arbitrary Sample Rate Converter)

### Background & Motivation
Different hardware and software components often demand specific, incompatible sample rates. For example:
- A sound card might require exactly 48,000 Hz for playback.
- A GSM cellular demodulator might strictly require an input rate of 270,833 Hz (the GSM symbol rate).
- The original SDR might have recorded the data at a fixed 2.048 MHz.

The `dsp_resample` tool uses an arbitrary rational polyphase resampler to bridge these gaps. It calculates the necessary interpolation and decimation factors to mathematically stretch or compress the waveform in the time-domain, allowing signals recorded on one piece of hardware to be seamlessly analyzed by a software pipeline demanding an entirely different sample rate paradigm.

---

## 6. `dsp_format` (Signal Formatting Utility)

### Background & Motivation
Digital signals can be represented in multiple forms (Real vs. Complex/Analytic) and multiple data types (8-bit integers, 16-bit integers, 32-bit floats, 64-bit doubles). 

**Real vs. Complex:**
While audio is typically Real (a single stream of amplitudes), RF processing is almost exclusively performed using Complex IQ data (In-Phase and Quadrature). Complex data naturally represents phase, allows for processing negative and positive frequencies independently, and avoids the mathematical mirroring (aliasing) that plagues Real signals at 0 Hz. 
The `dsp_format` tool can execute a Hilbert transform to mathematically deduce the phase of a Real signal, perfectly converting it into an Analytic Complex signal so it can be fed into advanced RF demodulators.

**Data Type Casting:**
SDR hardware uses analog-to-digital converters (ADCs) that output small integers (e.g., 8-bit or 12-bit) to save USB bandwidth. However, mathematical operations like filtering or FFTs require the high dynamic range of 32-bit floating-point numbers to prevent overflow or truncation. `dsp_format` serves as the vital bridge, seamlessly casting between storage-efficient integer formats and math-friendly floating-point formats.
