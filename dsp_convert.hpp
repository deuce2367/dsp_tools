#pragma once

#include <string>

// Run the format conversion pipeline
// Throws std::runtime_error on failure
void run_convert_pipeline(const std::string& input_file, 
                          const std::string& output_file, 
                          const std::string& format, 
                          double rate, 
                          double freq_mhz,
                          bool sigmf,
                          double timecode);
