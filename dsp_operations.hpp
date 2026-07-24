#pragma once
#include <string>

// dsp_filter
void run_filter_pipeline(const std::string& input_file, const std::string& output_file, const std::string& filter_type, double cutoff1, double cutoff2, size_t taps, double center_freq = 0.0);

// dsp_resample
void run_resample_pipeline(const std::string& input_file, const std::string& output_file, double new_rate, const std::string& quality_str);

// dsp_whitener
void run_whitener_pipeline(const std::string& input_file, const std::string& output_file, size_t fft_size, double alpha, double blank_threshold, size_t blank_window, double output_gain, double strength, const std::string& mode, double excess_leak);
