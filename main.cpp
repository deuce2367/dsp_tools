#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <nlohmann/json.hpp>

#include "dsp_engine.hpp"
#include "plot_generator.hpp"

int main(int argc, char** argv) {
    CLI::App app{"DSP Plotter - High Performance Signal Visualization"};

    std::string input_file;
    std::string format = "real";
    
    bool generate_examples = false;
    bool plot_fft = false;
    bool plot_waterfall = false;
    
    std::string output_name = "";
    std::string out_format = "jpg";
    int jpeg_quality = 75;
    int png_compression = 8;
    bool draw_grid = true;
    bool draw_labels = true;
    std::string data_type = "float32";
    
    int width = 512;
    int height = 512;
    size_t fft_size = 512;
    double stride_ratio = 0.0;
    std::string colormap = "gqrx";

    double center_freq = 0.0;
    double bandwidth = 0.0;
    double start_time = 0.0;
    double end_time = 0.0;
    double zoom_center = 0.0;
    double zoom_bw = 0.0;
    std::string window_type = "blackman-harris";
    
    std::string font_path = "/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf";
    
    double min_db = 1.0; // 1.0 indicates auto-level
    double max_db = 1.0; // 1.0 indicates auto-level
    
    int x_ticks = 0; // 0 indicates auto
    int y_ticks = 0; // 0 indicates auto
    int averaging = 4; // Default to 4 overlapping FFTs per step for smoothing
    std::string title = "";

    double box_start_time = -1.0;
    double box_duration = 0.0;
    double box_center_freq = 0.0;
    double box_bw = 0.0;
    std::string box_color = "white";

    app.add_flag("--generate-examples", generate_examples, "Generate example_real.bin and example_complex.bin test files");

    app.add_option("-i,--input", input_file, "Input binary file");
    app.add_option("-f,--format", format, "Data format: real or complex")->check(CLI::IsMember({"real", "complex"}));
    app.add_option("--data-type", data_type, "Input data type: float32, float64, int16, int8")->check(CLI::IsMember({"float32", "float64", "int16", "int8"}));
    
    app.add_flag("--plot-fft", plot_fft, "Generate a 1D FFT plot of the first frame");
    app.add_flag("--plot-waterfall", plot_waterfall, "Generate a 2D PSD Waterfall plot");

    auto opt_output = app.add_option("-o,--output", output_name, "Output filename (if not specified, derived from input by appending _waterfall/_fft)");    
    auto* opt_format = app.add_option("--out-format", out_format, "Output image format: png or jpg (default: jpg)")
        ->check(CLI::IsMember({"png", "jpg", "jpeg"}));
    
    app.add_option("--jpeg-quality", jpeg_quality, "JPEG compression quality from 1 to 100 (default: 75)")
        ->check(CLI::Range(1, 100));

    app.add_flag("--no-grid,!--grid", draw_grid, "Disable plotting grid lines");
    app.add_flag("--no-labels,!--labels", draw_labels, "Disable plotting axis labels");

    app.add_option("--width", width, "Output image width in pixels (default: 512)");
    app.add_option("--height", height, "Output image height in pixels (default: 512)");
    auto opt_fft_size = app.add_option("--fft-size", fft_size, "FFT size (default: 512)");
    auto opt_stride = app.add_option("--stride-ratio", stride_ratio, "Time stride ratio relative to FFT window. E.g. 0.5 is 50% overlap, 1.25 leaves gaps (default: auto-fit to image height)");
    app.add_option("-c,--colormap", colormap, "Colormap for waterfall (default: gqrx)")
        ->check(CLI::IsMember({"electric", "gqrx", "websdr", "pablo", "frog", "jet", "turbo", "grape"}));
    
    auto opt_center_freq = app.add_option("--center-freq", center_freq, "Original center frequency in MHz (for axis labels)");
    app.add_option("--bandwidth", bandwidth, "Original bandwidth in MHz (for axis labels)");
    
    app.add_option("--start-time", start_time, "Start time to process in seconds");
    app.add_option("--end-time", end_time, "End time to process in seconds (0 for EOF)");
    auto opt_zoom_center = app.add_option("--zoom-center", zoom_center, "Absolute zoom center frequency in MHz");
    double zoom_offset = 0.0;
    auto opt_zoom_offset = app.add_option("--zoom-offset", zoom_offset, "Zoom offset from center frequency in MHz");
    app.add_option("--zoom-bw", zoom_bw, "Zoom bandwidth in MHz (default: full bandwidth)");
    app.add_option("--window", window_type, "Window function: blackman-harris, hann, hamming, flattop, bartlett (default: blackman-harris)")->check(CLI::IsMember({"blackman-harris", "hann", "hamming", "flattop", "bartlett"}));
    
    app.add_option("--font", font_path, "Path to TTF font file for scalable text (default: /usr/share/fonts/truetype/noto/NotoMono-Regular.ttf)");
    app.add_option("--min-db", min_db, "Minimum dB level for plotting (default: auto)");
    app.add_option("--max-db", max_db, "Maximum dB level for plotting (default: auto)");
    app.add_option("--x-ticks", x_ticks, "Number of grid ticks on the X axis");
    app.add_option("--y-ticks", y_ticks, "Number of grid ticks on the Y axis");
    app.add_option("--averaging", averaging, "Number of overlapping FFTs to average per row (default: 4)")->check(CLI::PositiveNumber);
    app.add_option("--title", title, "Title of the plot (default: input filename)");

    app.add_option("--box-start-time", box_start_time, "Start time in seconds for highlight box (overlay)");
    app.add_option("--box-duration", box_duration, "Duration in seconds for highlight box (overlay)");
    app.add_option("--box-center-freq", box_center_freq, "Center frequency in MHz for highlight box (overlay)");
    app.add_option("--box-bw", box_bw, "Bandwidth in MHz for highlight box (overlay)");
    app.add_option("--box-color", box_color, "Color for highlight box: red, green, blue, yellow, white, cyan, magenta, black (default: white)");

    CLI11_PARSE(app, argc, argv);

    spdlog::info("====================================");
    spdlog::info("DSP Plotter - High Performance Engine");
    spdlog::info("====================================");

    if (generate_examples) {
        spdlog::info("Generating example_real.bin and example_complex.bin...");
        std::vector<float> real_out, cplx_out;
        for(int i=0; i<100000; ++i) {
            double t = i / 1e6;
            // Chirp + constant tone
            double phase = 2 * M_PI * (-200e3 * t + 0.5 * 400e3 / 0.1 * t * t);
            double re = std::cos(phase) + 0.5 * std::cos(2*M_PI*300e3*t);
            double im = std::sin(phase) + 0.5 * std::sin(2*M_PI*300e3*t);
            real_out.push_back(static_cast<float>(re));
            cplx_out.push_back(static_cast<float>(re)); 
            cplx_out.push_back(static_cast<float>(im));
        }
        std::ofstream out_r("example_real.bin", std::ios::binary);
        out_r.write(reinterpret_cast<const char*>(real_out.data()), real_out.size() * sizeof(float));
        std::ofstream out_c("example_complex.bin", std::ios::binary);
        out_c.write(reinterpret_cast<const char*>(cplx_out.data()), cplx_out.size() * sizeof(float));
        spdlog::info("Examples generated successfully!");
        return 0;
    }

    if (input_file.empty()) {
        spdlog::error("No input file specified. Use -i <file> or run with --generate-examples.");
        return 1;
    }

    if (output_name.empty()) {
        size_t last_slash = input_file.find_last_of("/\\");
        std::string basename = (last_slash == std::string::npos) ? input_file : input_file.substr(last_slash + 1);
        size_t last_dot = basename.find_last_of(".");
        if (last_dot != std::string::npos) {
            output_name = basename.substr(0, last_dot);
        } else {
            output_name = basename;
        }
    }
    
    if (title.empty()) {
        size_t last_slash = input_file.find_last_of("/\\");
        title = (last_slash == std::string::npos) ? input_file : input_file.substr(last_slash + 1);
    }

    if (!plot_fft && !plot_waterfall) {
        spdlog::info("Neither --plot-fft nor --plot-waterfall was specified. Defaulting to waterfall only.");
        plot_waterfall = true;
    }

    try {
        DspEngine dsp(fft_size);
        std::vector<std::vector<double>> spectrogram;
        std::vector<double> first_frame_mag;
        std::vector<double> freq_bins;
        
        spdlog::info("Processing file: {}", input_file);
        spdlog::stopwatch sw_dsp;
        
        // File Auto-detection
        bool is_wav = false;
        bool is_blue = false;
        std::string format_str = data_type;
        double file_sr = 1.0;
        int num_channels = 0;
        double timecode = 0.0;
        
        // SigMF Intercept
        bool is_sigmf = false;
        bool sigmf_set_center = false;
        if (input_file.size() > 11 && (input_file.substr(input_file.size() - 11) == ".sigmf-meta" || input_file.substr(input_file.size() - 11) == ".sigmf-data")) {
            is_sigmf = true;
            std::string meta_file = input_file.substr(0, input_file.size() - 11) + ".sigmf-meta";
            std::string data_file = input_file.substr(0, input_file.size() - 11) + ".sigmf-data";
            
            try {
                std::ifstream f(meta_file);
                nlohmann::json data = nlohmann::json::parse(f);
                
                if (data.contains("global")) {
                    auto& glob = data["global"];
                    if (glob.contains("core:sample_rate")) file_sr = glob["core:sample_rate"];
                    if (glob.contains("core:datatype")) {
                        std::string dt = glob["core:datatype"];
                        if (dt == "cf32_le") { format_str = "float32"; num_channels = 2; }
                        else if (dt == "ci16_le") { format_str = "int16"; num_channels = 2; }
                        else if (dt == "ci8_le" || dt == "cu8_le") { format_str = "int8"; num_channels = 2; }
                        else if (dt == "f32_le") { format_str = "float32"; num_channels = 1; }
                        else if (dt == "i16_le") { format_str = "int16"; num_channels = 1; }
                    }
                    if (glob.contains("core:hw")) {
                        spdlog::info("SigMF HW: {}", glob["core:hw"].get<std::string>());
                    }
                }
                
                if (data.contains("captures") && data.at("captures").is_array() && data.at("captures").size() > 0) {
                    auto& cap = data.at("captures")[0];
                    if (cap.contains("core:frequency")) {
                        center_freq = cap.at("core:frequency").get<double>() / 1e6;
                        sigmf_set_center = true;
                    }
                }
                
                input_file = data_file; // switch input to data file
                format = (num_channels == 2) ? "complex" : "real";
                if (bandwidth == 0.0) bandwidth = file_sr / 1e6;
                
                spdlog::info("SigMF Detected: SR: {} Hz, Center: {} MHz, Format: {}", file_sr, center_freq, format_str);
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse SigMF metadata: {}", e.what());
                is_sigmf = false;
            }
        }
        
        if (!is_sigmf) {
            dsp.get_file_info(input_file, num_channels, file_sr, is_wav, is_blue, format_str, timecode);
            
            if (is_wav || is_blue) {
                format = (num_channels == 2) ? "complex" : "real";
                if (bandwidth == 0.0) {
                    bandwidth = file_sr / 1e6;
                }
            } else {
                // raw fallback
                if (bandwidth > 0.0) file_sr = bandwidth * 1e6;
            }
        }
        
        if (opt_center_freq->count() == 0) {
            if (!sigmf_set_center && format == "real") {
                center_freq = bandwidth / 4.0; // The true physical center of the 0 to Fs/2 band
            }
        }
        
        if (x_ticks <= 0) x_ticks = std::max(1, width / 128);
        if (y_ticks <= 0) y_ticks = std::max(1, height / 64);
        
        DspEngine::StreamConfig config;
        config.filename = input_file;
        config.is_wav = is_wav;
        config.is_blue = is_blue;
        config.data_type = format == "complex" ? "complex" : data_type;
        config.start_time = start_time;
        config.end_time = end_time;
        config.original_timecode = timecode;
        config.sample_rate = file_sr;
        config.center_freq = center_freq;
        if (opt_zoom_offset->count() > 0) {
            config.zoom_center = center_freq + zoom_offset;
        } else if (opt_zoom_center->count() > 0) {
            config.zoom_center = zoom_center;
        } else {
            config.zoom_center = center_freq;
        }
        config.zoom_bw = zoom_bw;
        if (opt_fft_size->count() > 0) config.window_size = fft_size;
        else config.window_size = width;
        config.output_height = height;
        
        if (opt_stride->count() > 0 && stride_ratio > 0.0) {
            config.step_size = std::max<size_t>(1, fft_size * stride_ratio);
        }
        else config.step_size = 0; // auto
        config.output_width = width;
        config.output_height = height;
        config.time_smoothing = averaging;
        config.window_type = window_type;
        
        spdlog::info("====================================");
        spdlog::info("Configuration Summary:");
        spdlog::info("  Input File     : {}", input_file);
        spdlog::info("  Format         : {} ({})", format, is_wav ? "WAV Native" : (is_blue ? "BLUE Native" : format_str));
        spdlog::info("  Center Freq    : {:.3f} MHz", center_freq);
        spdlog::info("  Bandwidth      : {:.3f} MHz", bandwidth);
        spdlog::info("  Time Slice     : {:.2f}s to {}", start_time, end_time > 0 ? std::to_string(end_time) + "s" : "EOF");
        spdlog::info("  Dimensions     : {}x{}", width, height);
        spdlog::info("  Colormap       : {}", colormap);
        spdlog::info("  Grid/Labels    : {}/{}", draw_grid ? "Yes" : "No", draw_labels ? "Yes" : "No");
        if (min_db != 1.0 || max_db != 1.0) {
            spdlog::info("  DB Range       : Manual ({:.1f} to {:.1f} dB)", min_db, max_db);
        } else {
            spdlog::info("  DB Range       : Auto-leveling");
        }
        spdlog::info("====================================");
        
        DspEngine::StreamingResult result = dsp.process_file_streaming(config);
        
        spectrogram = result.spectrogram;
        first_frame_mag = result.first_frame_mag;
        size_t actual_step_size = result.actual_step_size;
        
        double fs = config.zoom_bw > 0.0 ? config.zoom_bw : bandwidth;
        double z_center = config.zoom_center != 0.0 ? config.zoom_center : center_freq;
        
        for (size_t i = 0; i < width; ++i) {
            freq_bins.push_back((z_center - fs/2.0) + (i * fs / width));
        }
        
        spdlog::info("DSP processing took {:.5f} seconds", sw_dsp);

        double actual_min_db = 0.0;
        double actual_max_db = -1000.0;
        double sum_db = 0.0;
        size_t count_db = 0;
        if (!spectrogram.empty()) {
            for (const auto& row : spectrogram) {
                for (double val : row) {
                    if (val < actual_min_db) actual_min_db = val;
                    if (val > actual_max_db) actual_max_db = val;
                    sum_db += val;
                    count_db++;
                }
            }
        }
        
        double mean_db = count_db > 0 ? (sum_db / count_db) : -100.0;
        
        double final_min_db = (min_db == 1.0) ? (mean_db - 15.0) : min_db; // Better auto-leveling
        double final_max_db = (max_db == 1.0) ? actual_max_db : max_db;

        if (plot_waterfall) {
            spdlog::stopwatch sw_plot;
            std::string wfile = opt_output->count() > 0 ? output_name : output_name + "_waterfall." + out_format;
            spdlog::info("Generating Fast Waterfall {} ({}x{}) (Range: {:.1f} to {:.1f} dB)...", out_format, width, height, final_min_db, final_max_db);
            
            double total_duration_sec = 0.0;
            if (file_sr > 0.0) {
                total_duration_sec = spectrogram.size() * actual_step_size / file_sr;
            }
            
            double z_center = result.actual_zoom_center;
            double fs = result.actual_zoom_bw;
            
            char start_time_buf[64];
            if (timecode > 0.0) {
                double abs_time = timecode + start_time;
                // Midas epoch is 1950-01-01. Unix is 1970-01-01.
                // Difference is 7305 days = 631152000 seconds.
                time_t unix_time = static_cast<time_t>(abs_time - 631152000.0);
                struct tm* tm_info = gmtime(&unix_time);
                if (tm_info) {
                    int millis = static_cast<int>(std::round((abs_time - std::floor(abs_time)) * 1000.0));
                    if (millis == 1000) {
                        millis = 0; // Edge case
                        // (Technically should add 1 second to unix_time, but this is minor)
                    }
                    char time_str[32];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);
                    snprintf(start_time_buf, sizeof(start_time_buf), "%s.%03d", time_str, millis);
                } else {
                    snprintf(start_time_buf, sizeof(start_time_buf), "%.2fs", abs_time);
                }
            } else {
                snprintf(start_time_buf, sizeof(start_time_buf), "%.2fs", start_time);
            }
            PlotGenerator::generate_fast_waterfall(spectrogram, wfile, width, height, colormap,
                                        final_min_db, final_max_db,
                                        z_center, fs, std::string(start_time_buf), total_duration_sec, draw_grid, draw_labels, out_format, x_ticks, y_ticks, title, jpeg_quality, png_compression, font_path,
                                        box_start_time, box_duration, box_center_freq, box_bw, box_color);
            spdlog::info("Waterfall rendered to {} in {:.5f} seconds", wfile, sw_plot);
        }
        
        if (plot_fft && !spectrogram.empty()) {
            spdlog::stopwatch sw_plot;
            std::string ffile = opt_output->count() > 0 ? output_name : output_name + "_fft." + out_format;
            spdlog::info("Generating Fast FFT Plot {} ({}x{}) (Range: {:.1f} to {:.1f} dB)...", out_format, width, height, final_min_db, final_max_db);
            PlotGenerator::generate_fast_fft_plot(freq_bins, first_frame_mag, ffile, width, height, final_min_db, final_max_db, z_center, fs, draw_grid, draw_labels, out_format, x_ticks, y_ticks, title, jpeg_quality, png_compression, colormap, font_path);
            spdlog::info("FFT rendered to {} in {:.5f} seconds", ffile, sw_plot);
        }


        spdlog::info("Done! Process complete.");
        
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }

    return 0;
}
