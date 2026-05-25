#pragma once

#include <vector>
#include <string>
#include <kfr/base.hpp>
class DspEngine {
public:
    DspEngine(size_t fft_size);
    ~DspEngine();

#pragma pack(push, 1)
    struct BlueHeader {
        char version[4];
        char head_rep[4];
        char data_rep[4];
        int32_t detached;
        int32_t protected_flag;
        int32_t pipe;
        int32_t ext_start;
        int32_t ext_size;
        double data_start;
        double data_size;
        int32_t type;
        char format[2];
        int16_t flagmask;
        double timecode;
        int16_t inlet;
        int16_t outlets;
        int32_t outmask;
        int32_t pipeloc;
        int32_t pipesize;
        double in_byte;
        double out_byte;
        double outbytes[8];
        int32_t keylength;
        char keywords[92];
        
        // Adjunct (Offset 256)
        double xstart;
        double xdelta;
        int32_t xunits;
        char padding[232];
    };
#pragma pack(pop)

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
