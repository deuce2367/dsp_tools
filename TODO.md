# DSP Tools - Future Ideas & TODOs

This document contains a list of future visualization ideas and enhancements to make the DSP Toolkit more comprehensive for digital signal analysis.

## Additional DSP Views to Implement

- [ ] **Constellation (Scatter) Plot**
  - **Description:** Plots the I channel on the X-axis against the Q channel on the Y-axis as a scatter plot.
  - **Use Case:** Absolutely mandatory for analyzing digital communications (e.g., QPSK, QAM). Allows visualization of the modulation scheme, phase noise, and symbol clusters.
  - **Implementation Note:** SigPlot natively supports 2D scatter/constellation plots for complex data.

- [ ] **Phase vs. Time / Frequency vs. Time**
  - **Description:** Instead of plotting Amplitude vs. Time, plot the instantaneous Phase angle (`atan2(Q, I)`) or its derivative (Frequency) over time.
  - **Use Case:** Extremely useful for analyzing FM (Frequency Modulation), PM (Phase Modulation) signals, radar chirps, or FSK (Frequency Shift Keying) telemetry.

- [ ] **Histogram (Probability Density)**
  - **Description:** A histogram showing the distribution of the signal amplitude or power values.
  - **Use Case:** Best way to quickly verify if the signal is pure Gaussian noise, if it's hitting ADC limits (clipping), or to set precise threshold levels for automated detection and squelch.

- [ ] **Eye Diagram**
  - **Description:** Overlaps sections of the time domain signal synchronized to the symbol rate.
  - **Use Case:** Strictly for digital signals. Helps evaluate signal quality, jitter, and Intersymbol Interference (ISI).

## Performance Enhancements

- [ ] **Multi-threading (OpenMP)**
  - Parallelize C++ min/max decimation loops across all CPU cores to speed up interactive plot generation on massive (multi-gigabyte) files.
- [ ] **Pre-computation / Caching**
  - Compute a decimated Min/Max overview of the entire file once when the file is first opened (similar to Audacity's `.au` peak files), allowing instantaneous load times when fully zoomed out on massive files.
