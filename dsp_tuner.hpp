#pragma once
#include <string>

// Quality levels for the resampler
enum class TunerQuality {
    Draft,
    Low,
    Normal,
    High,
    Perfect
};

/**
 * Digitally down converts and optimally decimates a signal.
 * 
 * @param input_file The path to the input bluefile.
 * @param output_file The path to the output bluefile to create.
 * @param center The target center frequency to tune to in Hz.
 * @param bandwidth The target bandwidth in Hz. Output sample rate will match this.
 * @param start_time Start time in seconds relative to the file start (0.0 = beginning).
 * @param duration Duration in seconds to process (0.0 = until end of file).
 * @param file_center Optional override for the original file center frequency.
 * @param file_center_provided True if the file_center parameter should be used.
 * @param quality Resampler quality level.
 */
void run_tuner_pipeline(
    const std::string& input_file, 
    const std::string& output_file, 
    double center, 
    double bandwidth,
    double start_time,
    double duration,
    double file_center, 
    bool file_center_provided, 
    TunerQuality quality,
    int oversample_factor
);
