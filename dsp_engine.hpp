#pragma once

#include <vector>
#include <string>
#include <kfr/base.hpp>
#include "bluefile_io.hpp"

class DspEngine {
public:
    DspEngine(size_t fft_size);
    ~DspEngine();



    struct StreamConfig {
        std::string filename;
        bool is_wav = false;
        bool is_blue = false;
        std::string data_type = "float32"; // for raw binary
        
        double start_time = 0.0;
        double end_time = 0.0; // 0 means end of file
        double original_timecode = 0.0; // Timecode from file header
        
        double sample_rate = 1.0;
        double center_freq = 0.0;
        double zoom_center = 0.0; // 0 means same as center_freq
        double zoom_bw = 0.0; // 0 means same as bandwidth
        
        size_t window_size = 0; // 0 means auto
        size_t step_size = 0; // 0 means auto
        size_t output_width = 1024;
        size_t output_height = 512;
        int time_smoothing = 1;
        std::string window_type = "blackman-harris";
    };

    struct StreamingResult {
        std::vector<std::vector<double>> spectrogram;
        std::vector<double> first_frame_mag;
        std::vector<double> avg_fft;
        std::vector<double> max_hold_fft;
        std::vector<double> min_hold_fft; 
        size_t actual_step_size = 0;
        double actual_zoom_center = 0.0;
        double actual_zoom_bw = 0.0;
        double original_start_time = 0.0;
    };

    StreamingResult process_file_streaming(const StreamConfig& config);

    // Compute FFT magnitude in dB from real samples
    std::vector<double> compute_fft_magnitude_db(const std::vector<double>& real_samples);
    
    // File Ingestion
    void get_file_info(const std::string& filename, int& channels, double& sample_rate, bool& is_wav, bool& is_blue, std::string& format_str, double& timecode);
    
private:
    size_t m_fft_size;
};
