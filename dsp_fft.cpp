#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include "dsp_engine.hpp"
#include "bluefile_io.hpp"
#include <cstring>

#ifndef DSP_TOOLS_TEST_MODE

int main(int argc, char** argv) {
    CLI::App app{"DSP FFT Extractor (Outputs Type 1000 Bluefile)"};

    std::string input_file;
    std::string output_file;
    double center_freq = 0.0;
    double zoom_center = 0.0;
    double zoom_bw = 0.0;
    size_t window_size = 1024;
    double start_time = 0.0;
    double duration = 0.0;
    int smoothing = 1;
    std::string mode = "avg"; // avg or max

    app.add_option("-i,--input", input_file, "Input file (Blue/WAV)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (Type 1000)")->required();
    app.add_option("-c,--center-freq", center_freq, "Center frequency of input file (Hz) (default: 0 or auto from header)");
    app.add_option("--zoom-center", zoom_center, "Zoom center frequency (Hz)");
    app.add_option("--zoom-bw", zoom_bw, "Zoom bandwidth (Hz)");
    app.add_option("-w,--window-size", window_size, "FFT size (default: 1024)");
    app.add_option("--smoothing", smoothing, "Number of FFTs to average (default: 1)");
    app.add_option("-s,--start", start_time, "Start time (seconds)");
    app.add_option("-d,--duration", duration, "Duration to process (seconds)");
    app.add_option("-m,--mode", mode, "FFT mode ('avg' or 'max') (default: avg)")->check(CLI::IsMember({"avg", "max"}));

    CLI11_PARSE(app, argc, argv);

    try {
        spdlog::info("Initializing FFT Extraction...");
        auto t_start = std::chrono::high_resolution_clock::now();

        DspEngine engine(window_size);
        DspEngine::StreamConfig config;
        config.filename = input_file;
        
        int channels;
        double sample_rate;
        bool is_wav;
        bool is_blue;
        std::string format_str;
        double timecode;
        engine.get_file_info(input_file, channels, sample_rate, is_wav, is_blue, format_str, timecode);

        config.is_wav = is_wav;
        config.is_blue = is_blue;
        config.sample_rate = sample_rate;
        config.center_freq = center_freq;
        config.zoom_center = zoom_center;
        config.zoom_bw = zoom_bw;
        config.window_size = window_size;
        config.output_width = window_size; // Not really used for image here, but maps to bins
        config.time_smoothing = smoothing;
        config.start_time = start_time;
        if (duration > 0) {
            config.end_time = start_time + duration;
        } else {
            config.end_time = 0;
        }

        auto result = engine.process_file_streaming(config);

        const std::vector<double>& out_data = (mode == "max") ? result.max_hold_fft : result.avg_fft;
        
        // Write Type 1000 Bluefile
        BlueHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        std::strncpy(hdr.version, "BLUE", 4);
        std::strncpy(hdr.head_rep, "EEEI", 4);
        std::strncpy(hdr.data_rep, "EEEI", 4);
        hdr.ext_start = 0;
        hdr.ext_size = 0;
        hdr.type = 1000;
        hdr.format[0] = 'S'; hdr.format[1] = 'F'; // saving as float
        hdr.timecode = result.original_start_time;
        hdr.data_start = 512.0;
        hdr.data_size = out_data.size() * sizeof(float);
        
        // Setup adjunct for Type 1000
        hdr.xstart = result.actual_zoom_center - (result.actual_zoom_bw / 2.0);
        hdr.xdelta = result.actual_zoom_bw / out_data.size();
        hdr.xunits = 2; // Hz

        write_bluefile_header(output_file, hdr);
        
        // Write binary data (convert to float)
        int fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
        if (fd < 0) throw std::runtime_error("Cannot open output file to append data");
        
        std::vector<float> out_f(out_data.size());
        for (size_t i = 0; i < out_data.size(); ++i) {
            out_f[i] = static_cast<float>(out_data[i]);
        }
        
        if (write(fd, out_f.data(), out_f.size() * sizeof(float)) < 0) {
            close(fd);
            throw std::runtime_error("Failed to write to file.");
        }
        close(fd);

        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> t_diff = t_end - t_start;
        spdlog::info("Extracted FFT successfully to {}. Size: {} bins. Time: {:.3f} s", output_file, out_data.size(), t_diff.count());

    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }

    return 0;
}

#endif
