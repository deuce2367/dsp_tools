#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <nlohmann/json.hpp>

#include "dsp_engine.hpp"
#include "plot_generator.hpp"

static size_t get_file_size_bytes(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && st.st_size >= 0) {
        return static_cast<size_t>(st.st_size);
    }
    return 0;
}

static std::string format_byte_size(size_t bytes) {
    std::ostringstream ss;
    if (bytes >= 1024ULL * 1024ULL) {
        ss << bytes << " bytes (" << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB)";
    } else if (bytes >= 1024ULL) {
        ss << bytes << " bytes (" << std::fixed << std::setprecision(2) << (bytes / 1024.0) << " KB)";
    } else {
        ss << bytes << " bytes";
    }
    return ss.str();
}

static std::string format_j1950_utc(double j1950_sec) {
    if (j1950_sec <= 0.0) return "None";
    time_t unix_time = static_cast<time_t>(j1950_sec - 631152000.0);
    struct tm tm_buf;
    struct tm* tm_info = gmtime_r(&unix_time, &tm_buf);
    if (!tm_info) return std::to_string(j1950_sec) + "s";
    int millis = static_cast<int>(std::round((j1950_sec - std::floor(j1950_sec)) * 1000.0));
    if (millis >= 1000) millis = 999;
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", tm_info);
    char out_buf[64];
    snprintf(out_buf, sizeof(out_buf), "%s.%03dZ", time_str, millis);
    return std::string(out_buf);
}

int main(int argc, char** argv) {
    CLI::App app{"DSP Plotter - High Performance Signal Visualization"};

    std::string input_file;
    std::string format = "real";
    
    bool generate_examples = false;
    bool plot_fft = false;
    bool plot_waterfall = false;
    
    std::string output_name = "";
    std::string out_format = "jpg";
    int image_quality = 50;
    int png_compression = -1;
    bool draw_grid = true;
    bool draw_labels = true;
    std::string data_type = "float32";
    bool peak_detection = false;
    bool visualize_peaks = false;
    std::string visualize_mode = "cell";
    double peak_threshold = 15.0;
    
    int width = 512;
    int height = 512;
    size_t fft_size = 512;
    size_t overlap = 0;
    double stride_ratio = 0.0;
    std::string colormap = "gqrx";
    std::string theme = "dark";
    std::string fft_color = "";
    std::string fft_fill = "gradient";
    float fft_fill_opacity = 0.5f;

    double center_freq = 0.0;
    double bandwidth = 0.0;
    double start_time = 0.0;
    double end_time = 0.0;
    double zoom_center = 0.0;
    double zoom_bw = 0.0;
    std::string window_type = "blackman-harris";
    
    std::string font_path = "/usr/share/fonts/truetype/noto/NotoMono-Regular.ttf";
    int font_size = 12;
    
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
    float box_alpha = 0.3f;
    int box_size = 1;

    app.add_flag("--generate-examples", generate_examples, "Generate example_real.bin and example_complex.bin test files");

    app.add_option("-i,--input", input_file, "Input binary file");
    auto* opt_data_format = app.add_option("-f,--format", format, "Data format: real or complex")->check(CLI::IsMember({"real", "complex"}));
    app.add_option("--data-type", data_type, "Input data type: float32, float64, int16, int8")->check(CLI::IsMember({"float32", "float64", "int16", "int8"}));
    
    app.add_flag("--plot-fft", plot_fft, "Generate a 1D FFT plot of the first frame");
    app.add_flag("--plot-waterfall", plot_waterfall, "Generate a 2D PSD Waterfall plot");

    auto opt_output = app.add_option("-o,--output", output_name, "Output filename (if not specified, derived from input by appending _waterfall/_fft)");    
    auto* opt_format = app.add_option("--out-format", out_format, "Output image format: png, jpg, webp, or avif (default: jpg)")
        ->check(CLI::IsMember({"png", "jpg", "jpeg", "webp", "avif"}, CLI::ignore_case));
    
    app.add_option("--image-quality,--jpeg-quality", image_quality, "Image compression/quality from 1 to 100 across formats (default: 50)")
        ->check(CLI::Range(1, 100));
    auto* opt_png_comp = app.add_option("--png-compression", png_compression, "Explicit PNG zlib compression level from 0 (none) to 9 (max) (default: auto from --image-quality)")
        ->check(CLI::Range(0, 9));

    app.add_flag("--grid,--draw-grid,!--no-grid", draw_grid, "Enable/disable plotting grid lines (default: enabled)");
    app.add_flag("--labels,--draw-labels,!--no-labels", draw_labels, "Enable/disable plotting axis labels (default: enabled)");

    app.add_flag("--peak-detection", peak_detection, "Enable squelch signal peak detection output to CSV");
    app.add_flag("--visualize-peaks", visualize_peaks, "Draw boxes/outlines around detected peak cells on the waterfall plot (uses --peak-threshold, --box-color, --box-alpha)");
    auto* opt_vis_mode = app.add_option("--visualize-mode", visualize_mode, "Peak visualization style: 'cell' (individual cell boxes) or 'outline' (merged energy contour) (default: cell)")
        ->check(CLI::IsMember({"cell", "outline"}, CLI::ignore_case));
    app.add_option("--peak-threshold", peak_threshold, "Signal detection threshold in dB above noise floor (default: 15.0)");

    app.add_option("--width", width, "Output image width in pixels (default: 512)");
    app.add_option("--height", height, "Output image height in pixels (default: 512)");
    auto opt_fft_size = app.add_option("--fft-size", fft_size, "FFT size (default: 512)");
    auto opt_stride = app.add_option("--stride-ratio", stride_ratio, "Time stride ratio relative to FFT window. E.g. 0.5 is 50% overlap, 1.25 leaves gaps (default: auto-fit to image height)");
    auto opt_overlap = app.add_option("--overlap", overlap, "FFT window overlap in samples (alternative to --stride-ratio)");
    app.add_option("-c,--colormap", colormap, "Colormap for waterfall/FFT (default: gqrx)")
        ->check(CLI::IsMember({"electric", "gqrx", "websdr", "pablo", "frog", "jet", "turbo", "grape", "greyscale", "sunset", "hot", "cold"}));
    app.add_option("--theme", theme, "Plot visual theme: dark or light (default: dark)")
        ->check(CLI::IsMember({"dark", "light"}));
    app.add_option("--fft-color,--fill-color", fft_color, "FFT line/solid-fill color: name (red, cyan, etc.), hex (#RRGGBB), or RGB (R,G,B) (default: colormap)");
    app.add_option("--fft-fill,--fill-mode", fft_fill, "FFT under-curve fill mode or color: gradient, solid, none, or color (default: gradient)");
    app.add_option("--fft-fill-opacity", fft_fill_opacity, "FFT under-curve fill opacity from 0.0 to 1.0 (default: 0.5)")
        ->check(CLI::Range(0.0f, 1.0f));
    
    auto opt_center_freq = app.add_option("--center-freq", center_freq, "Original center frequency in MHz (for axis labels)");
    auto opt_bandwidth = app.add_option("--bandwidth", bandwidth, "Original bandwidth in MHz (for axis labels)");
    
    app.add_option("--start-time", start_time, "Start time to process in seconds");
    app.add_option("--end-time", end_time, "End time to process in seconds (0 for EOF)");
    auto opt_zoom_center = app.add_option("--zoom-center", zoom_center, "Absolute zoom center frequency in MHz");
    double zoom_offset = 0.0;
    auto opt_zoom_offset = app.add_option("--zoom-offset", zoom_offset, "Zoom offset from center frequency in MHz");
    app.add_option("--zoom-bw", zoom_bw, "Zoom bandwidth in MHz (default: full bandwidth)");
    app.add_option("--window", window_type, "Window function: blackman-harris, hann, hamming, flattop, bartlett (default: blackman-harris)")->check(CLI::IsMember({"blackman-harris", "hann", "hamming", "flattop", "bartlett"}));
    
    auto* opt_font = app.add_option("--font", font_path, "Path to TTF font file for scalable text (default: /usr/share/fonts/truetype/noto/NotoMono-Regular.ttf)");
    app.add_option("--font-size", font_size, "Base font size in pixels for labels and titles (default: 12)")
        ->check(CLI::Range(4, 72));
    app.add_option("--min-db", min_db, "Minimum dB level for plotting (default: auto)");
    app.add_option("--max-db", max_db, "Maximum dB level for plotting (default: auto)");
    auto* opt_x_ticks = app.add_option("--x-ticks", x_ticks, "Number of grid ticks on the X axis (default: auto)");
    auto* opt_y_ticks = app.add_option("--y-ticks", y_ticks, "Number of grid ticks on the Y axis (default: auto)");
    app.add_option("--averaging", averaging, "Number of overlapping FFTs to average per row (default: 4)")->check(CLI::PositiveNumber);
    app.add_option("--title", title, "Title of the plot (default: input filename)");

    app.add_option("--box-start-time", box_start_time, "Start time in seconds for highlight box (overlay)");
    app.add_option("--box-duration", box_duration, "Duration in seconds for highlight box (overlay)");
    app.add_option("--box-center-freq", box_center_freq, "Center frequency in MHz for highlight box (overlay)");
    app.add_option("--box-bw", box_bw, "Bandwidth in MHz for highlight box (overlay)");
    app.add_option("--box-color", box_color, "Color for highlight box: name (red, green, blue, yellow, white, cyan, magenta, black), hex (#RRGGBB), or RGB (R,G,B) (default: white)");
    app.add_option("--box-alpha,--box-opacity", box_alpha, "Fill opacity/alpha for highlight box from 0.0 (outline only) to 1.0 (solid) (default: 0.3)")
        ->check(CLI::Range(0.0f, 1.0f));
    app.add_option("--box-size", box_size, "Line thickness in pixels for highlight and peak boxes/outlines (default: 1)")
        ->check(CLI::Range(1, 50));

    if (argc == 1) {
        std::cout << app.help() << std::endl;
        return 0;
    }
    
    CLI11_PARSE(app, argc, argv);

    spdlog::stopwatch sw_total;
    spdlog::info("==================================================================");
    spdlog::info("DSP Plotter - High Performance Signal Visualization Engine");
    spdlog::info("==================================================================");

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

    bool format_inferred_from_ext = false;
    for (char& c : out_format) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (out_format == "jpeg") out_format = "jpg";
    for (char& c : visualize_mode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (opt_vis_mode->count() > 0) {
        visualize_peaks = true;
    }

    if (opt_format->count() == 0 && opt_output->count() > 0) {
        size_t dot = output_name.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = output_name.substr(dot + 1);
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext == "jpeg") ext = "jpg";
            if (ext == "png" || ext == "jpg" || ext == "webp" || ext == "avif") {
                out_format = ext;
                format_inferred_from_ext = true;
            }
        }
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
    
    bool default_title_used = false;
    if (title.empty()) {
        size_t last_slash = input_file.find_last_of("/\\");
        title = (last_slash == std::string::npos) ? input_file : input_file.substr(last_slash + 1);
        default_title_used = true;
    }

    bool defaulted_to_waterfall = false;
    if (!plot_fft && !plot_waterfall) {
        spdlog::info("Neither --plot-fft nor --plot-waterfall was specified. Defaulting to waterfall only.");
        plot_waterfall = true;
        defaulted_to_waterfall = true;
    }

    try {
        size_t effective_fft_size = (opt_fft_size->count() > 0) ? fft_size : static_cast<size_t>(width);
        DspEngine dsp(effective_fft_size);
        std::vector<std::vector<double>> spectrogram;
        std::vector<double> first_frame_mag;
        std::vector<double> freq_bins;
        
        spdlog::stopwatch sw_dsp;
        
        // File Auto-detection
        bool is_wav = false;
        bool is_blue = false;
        std::string format_str = data_type;
        double file_sr = 1.0;
        int num_channels = (format == "complex") ? 2 : 1;
        double timecode = 0.0;
        double header_cf = 0.0;
        std::string cf_source = (opt_center_freq->count() > 0) ? "CLI (--center-freq)" : "Default (0.0 MHz)";
        std::string bw_source = (opt_bandwidth->count() > 0) ? "CLI (--bandwidth)" : "Auto";
        std::string sigmf_hw = "";
        
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
                        sigmf_hw = glob["core:hw"].get<std::string>();
                    }
                }
                
                if (data.contains("captures") && data.at("captures").is_array() && data.at("captures").size() > 0) {
                    auto& cap = data.at("captures")[0];
                    if (cap.contains("core:frequency") && opt_center_freq->count() == 0) {
                        center_freq = cap.at("core:frequency").get<double>() / 1e6;
                        sigmf_set_center = true;
                        cf_source = "SigMF Metadata";
                    }
                }
                
                input_file = data_file; // switch input to data file
                format = (num_channels == 2) ? "complex" : "real";
                if (bandwidth == 0.0) {
                    bandwidth = file_sr / 1e6;
                    bw_source = "SigMF Sample Rate";
                }
            } catch (const std::exception& e) {
                spdlog::warn("Failed to parse SigMF metadata: {}", e.what());
                is_sigmf = false;
            }
        }
        
        if (!is_sigmf) {
            dsp.get_file_info(input_file, num_channels, file_sr, is_wav, is_blue, format_str, timecode, header_cf);
            
            if (is_wav || is_blue) {
                format = (num_channels == 2) ? "complex" : "real";
                if (bandwidth == 0.0) {
                    bandwidth = file_sr / 1e6;
                    bw_source = is_wav ? "WAV Sample Rate" : "BLUE Header (1/xdelta)";
                }
                if (opt_center_freq->count() == 0 && header_cf != 0.0) {
                    center_freq = header_cf;
                    cf_source = "BLUE Header (RF_FREQUENCY_MHZ)";
                }
            } else {
                // raw fallback
                num_channels = (format == "complex") ? 2 : 1;
                format_str = data_type;
                if (bandwidth > 0.0) {
                    file_sr = bandwidth * 1e6;
                } else {
                    bandwidth = file_sr / 1e6;
                    bw_source = "Default (1.0 MSps)";
                }
            }
        }
        
        if (opt_center_freq->count() == 0) {
            if (!sigmf_set_center && header_cf == 0.0 && format == "real") {
                center_freq = bandwidth / 4.0; // The true physical center of the 0 to Fs/2 band
                cf_source = "Real Nyquist Center (Fs/4)";
            }
        }
        
        bool auto_x_ticks = (opt_x_ticks->count() == 0 || x_ticks <= 0);
        bool auto_y_ticks = (opt_y_ticks->count() == 0 || y_ticks <= 0);
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
        config.window_size = effective_fft_size;
        config.output_height = height;
        
        std::string stride_mode_desc = "Auto-fit to output height";
        if (opt_stride->count() > 0 && stride_ratio > 0.0) {
            config.step_size = std::max<size_t>(1, static_cast<size_t>(effective_fft_size * stride_ratio));
            std::ostringstream ss;
            ss << "Manual --stride-ratio " << std::fixed << std::setprecision(3) << stride_ratio
               << " (" << config.step_size << " samples/step)";
            stride_mode_desc = ss.str();
        } else if (opt_overlap->count() > 0) {
            config.step_size = (effective_fft_size > overlap) ? (effective_fft_size - overlap) : 1;
            std::ostringstream ss;
            ss << "Manual --overlap " << overlap << " samples (" << config.step_size << " samples/step)";
            stride_mode_desc = ss.str();
        } else {
            config.step_size = 0; // auto
        }
        config.output_width = width;
        config.output_height = height;
        config.time_smoothing = averaging;
        config.window_type = window_type;

        // Calculate input file metrics
        size_t input_bytes = get_file_size_bytes(input_file);
        size_t header_bytes = is_blue ? 512 : (is_wav ? 44 : 0);
        size_t bytes_per_component = 4;
        if (format_str == "int8") bytes_per_component = 1;
        else if (format_str == "int16") bytes_per_component = 2;
        else if (format_str == "float32" || format_str == "int32") bytes_per_component = 4;
        else if (format_str == "float64") bytes_per_component = 8;
        size_t bytes_per_sample = bytes_per_component * static_cast<size_t>(std::max(1, num_channels));
        size_t payload_bytes = (input_bytes > header_bytes) ? (input_bytes - header_bytes) : 0;
        size_t est_total_samples = (bytes_per_sample > 0) ? (payload_bytes / bytes_per_sample) : 0;
        double est_file_duration_sec = (file_sr > 0.0) ? (static_cast<double>(est_total_samples) / file_sr) : 0.0;

        std::string container_desc = is_sigmf ? "SigMF (.sigmf-meta/.sigmf-data)"
                                   : (is_blue ? "X-Midas BLUE (.prm/.tmp)"
                                   : (is_wav  ? "WAV Audio/IQ (.wav)"
                                              : "Raw Binary"));
        if (!is_sigmf && !is_blue && !is_wav && opt_data_format->count() == 0) {
            container_desc += " (default real)";
        }

        // Font status check
        bool external_font_exists = (get_file_size_bytes(font_path) > 0);
        int text_scale = std::max(1, std::min(width, height) / 800);
        int rendered_font_px = font_size * text_scale;

        // Effective codec description
        std::string fmt_upper = out_format;
        for (char& c : fmt_upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        std::string codec_details;
        if (out_format == "jpg") {
            codec_details = "JPEG Lossy (YUV 4:2:0, quality " + std::to_string(image_quality) + "/100)";
        } else if (out_format == "webp") {
            codec_details = "WebP Lossy (YUV 4:2:0, quality " + std::to_string(image_quality) + "/100)";
        } else if (out_format == "avif") {
            int eff_avif = PlotGenerator::effective_avif_quality(image_quality);
            codec_details = "AVIF Lossy (AV1 YUV 4:2:0, normalized quality " + std::to_string(image_quality) +
                            "/100 -> libavif quality " + std::to_string(eff_avif) + "/100, speed 6)";
        } else if (out_format == "png") {
            int eff_png = PlotGenerator::effective_png_compression(image_quality, png_compression);
            codec_details = "PNG Lossless RGB24 (zlib compression level " + std::to_string(eff_png) + "/9, " +
                            (opt_png_comp->count() > 0 ? "explicit --png-compression" : ("mapped from --image-quality " + std::to_string(image_quality))) + ")";
        }

        std::string active_plots_str;
        if (plot_waterfall && plot_fft) active_plots_str = "Waterfall (2D PSD) + FFT (1D Spectrum)";
        else if (plot_waterfall) active_plots_str = defaulted_to_waterfall ? "Waterfall (2D PSD, default)" : "Waterfall (2D PSD)";
        else if (plot_fft) active_plots_str = "FFT (1D Spectrum)";
        if (peak_detection) active_plots_str += " + Peak Detection CSV";
        if (visualize_peaks) active_plots_str += " + Visualize Peaks (" + visualize_mode + ")";

        bool box_active = (box_start_time >= 0.0 && box_duration > 0.0 && box_bw > 0.0);
        bool zoom_active = (zoom_bw > 0.0 || opt_zoom_center->count() > 0 || opt_zoom_offset->count() > 0);
        size_t raw_rgb_bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 3ULL;

        spdlog::info("Input & Signal Metadata:");
        spdlog::info("  Input File     : {} ({})", input_file, format_byte_size(input_bytes));
        spdlog::info("  Container      : {}", container_desc);
        if (!sigmf_hw.empty()) {
            spdlog::info("  SigMF Hardware : {}", sigmf_hw);
        }
        spdlog::info("  Signal Format  : {} ({} ch, {}, {} bytes/sample)", format, std::max(1, num_channels), format_str, bytes_per_sample);
        spdlog::info("  Sample Rate    : {:.6f} MSps ({:.1f} Hz)", file_sr / 1e6, file_sr);
        spdlog::info("  Total Samples  : {} samples ({:.3f}s total duration)", est_total_samples, est_file_duration_sec);
        spdlog::info("  Center Freq    : {:.6f} MHz [{}]", center_freq, cf_source);
        spdlog::info("  Bandwidth      : {:.6f} MHz [{:.6f} to {:.6f} MHz] [{}]",
                     bandwidth, center_freq - bandwidth / 2.0, center_freq + bandwidth / 2.0, bw_source);
        if (timecode > 0.0) {
            spdlog::info("  Timecode (UTC) : {} (J1950: {:.3f}s)", format_j1950_utc(timecode), timecode);
        } else {
            spdlog::info("  Timecode       : None (relative 0.00s)");
        }

        spdlog::info("DSP & Spectral Configuration:");
        spdlog::info("  Active Outputs : {}", active_plots_str);
        spdlog::info("  Time Slice     : {:.3f}s to {}", start_time, end_time > 0.0 ? (std::to_string(end_time) + "s") : "EOF");
        if (zoom_active) {
            double eff_zbw = (zoom_bw > 0.0) ? zoom_bw : bandwidth;
            spdlog::info("  Zoom / DDC     : Active -> Center: {:.6f} MHz (offset {:+.6f} MHz), Span: {:.6f} MHz [{:.6f} to {:.6f} MHz]",
                         config.zoom_center, config.zoom_center - center_freq, eff_zbw,
                         config.zoom_center - eff_zbw / 2.0, config.zoom_center + eff_zbw / 2.0);
        } else {
            spdlog::info("  Zoom / DDC     : Full Bandwidth (no zoom)");
        }
        spdlog::info("  FFT Parameters : {} bins ({}), Window: {}, Averaging: {} frames/row",
                     effective_fft_size, (opt_fft_size->count() > 0 ? "manual --fft-size" : "auto from width"),
                     window_type, averaging);
        spdlog::info("  Time Stride    : {}", stride_mode_desc);

        spdlog::info("Plot Styling & Encoding Options:");
        spdlog::info("  Image Canvas   : {}x{} px ({} pixels, raw RGB24: {})", width, height, width * height, format_byte_size(raw_rgb_bytes));
        spdlog::info("  Output Format  : {} ({}) -> {}", fmt_upper,
                     (opt_format->count() > 0 ? "CLI --out-format" : (format_inferred_from_ext ? "inferred from -o extension" : "default")),
                     codec_details);
        spdlog::info("  Theme/Colormap : Theme: {}, Colormap: {}", theme, colormap);
        if (plot_fft) {
            spdlog::info("  FFT Styling    : Fill: {}, Line/Fill Color: {}, Fill Opacity: {:.2f}",
                         fft_fill, fft_color.empty() ? ("colormap (" + colormap + ")") : fft_color, fft_fill_opacity);
        }
        spdlog::info("  Grid & Ticks   : Grid: {}, Labels: {}, X-Ticks: {} ({}), Y-Ticks: {} ({})",
                     draw_grid ? "Enabled" : "Disabled", draw_labels ? "Enabled" : "Disabled",
                     x_ticks, auto_x_ticks ? "auto" : "manual",
                     y_ticks, auto_y_ticks ? "auto" : "manual");
        spdlog::info("  Typography     : Base Font Size: {}px (scale {}x -> {}px effective), Font: {} ({})",
                     font_size, text_scale, rendered_font_px,
                     external_font_exists ? font_path : "Embedded DejaVuSansMono",
                     external_font_exists ? (opt_font->count() > 0 ? "CLI --font" : "system default TTF") : "fallback embedded TTF");
        spdlog::info("  Plot Title     : \"{}\" ({})", title, default_title_used ? "auto from filename" : "CLI --title");
        if (min_db != 1.0 || max_db != 1.0) {
            spdlog::info("  dB Scale Mode  : Manual (min: {}, max: {})",
                         min_db == 1.0 ? "auto" : (std::to_string(min_db) + " dB"),
                         max_db == 1.0 ? "auto" : (std::to_string(max_db) + " dB"));
        } else {
            spdlog::info("  dB Scale Mode  : Auto-leveling (min: mean - 15 dB, max: peak dB)");
        }
        if (box_active) {
            spdlog::info("  Highlight Box  : Active -> Time: {:.3f}s to {:.3f}s (dur: {:.3f}s), Freq: {:.6f} MHz (BW: {:.6f} MHz [{:.6f}..{:.6f} MHz]), Color: {}, Alpha: {:.2f}, Line Size: {}px",
                         box_start_time, box_start_time + box_duration, box_duration,
                         box_center_freq, box_bw, box_center_freq - box_bw / 2.0, box_center_freq + box_bw / 2.0,
                         box_color, box_alpha, box_size);
        } else {
            spdlog::info("  Highlight Box  : Disabled");
        }
        if (peak_detection || visualize_peaks) {
            spdlog::info("  Peak Detector  : Active (Threshold: +{:.1f} dB above noise floor, CSV: {}, Visualize: {} [mode: {}], Color: {}, Alpha: {:.2f}, Line Size: {}px)",
                         peak_threshold, peak_detection ? "Yes" : "No", visualize_peaks ? "Yes" : "No", visualize_mode, box_color, box_alpha, box_size);
        }
        spdlog::info("==================================================================");
        
        DspEngine::StreamingResult result = dsp.process_file_streaming(config);
        
        spectrogram = result.spectrogram;
        first_frame_mag = result.first_frame_mag;
        size_t actual_step_size = result.actual_step_size;
        
        double fs = result.actual_zoom_bw > 0.0 ? result.actual_zoom_bw : (config.zoom_bw > 0.0 ? config.zoom_bw : bandwidth);
        double z_center = result.actual_zoom_center != 0.0 ? result.actual_zoom_center : (config.zoom_center != 0.0 ? config.zoom_center : center_freq);
        
        size_t num_rows = spectrogram.size();
        size_t num_bins = (!spectrogram.empty()) ? spectrogram[0].size() : static_cast<size_t>(width);
        for (size_t i = 0; i < num_bins; ++i) {
            freq_bins.push_back((z_center - fs / 2.0) + (i * fs / num_bins));
        }
        
        double dsp_elapsed = sw_dsp.elapsed().count();

        double actual_min_db = 1e9;
        double actual_max_db = -1e9;
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
        } else {
            actual_min_db = -100.0;
            actual_max_db = -20.0;
        }
        
        double mean_db = count_db > 0 ? (sum_db / count_db) : -100.0;
        double noise_floor_db = mean_db;
        if (!result.avg_fft.empty()) {
            std::vector<double> sorted_avg = result.avg_fft;
            std::sort(sorted_avg.begin(), sorted_avg.end());
            noise_floor_db = sorted_avg[sorted_avg.size() / 2];
        }
        
        double final_min_db = (min_db == 1.0) ? (mean_db - 15.0) : min_db;
        double final_max_db = (max_db == 1.0) ? actual_max_db : max_db;

        double total_duration_sec = 0.0;
        double step_ms = 0.0;
        if (file_sr > 0.0) {
            total_duration_sec = num_rows * actual_step_size / file_sr;
            step_ms = (static_cast<double>(actual_step_size) / file_sr) * 1000.0;
        }
        double freq_res_hz = (num_bins > 0) ? ((fs * 1e6) / static_cast<double>(num_bins)) : 0.0;
        double effective_stride_ratio = (effective_fft_size > 0) ? (static_cast<double>(actual_step_size) / static_cast<double>(effective_fft_size)) : 0.0;
        double effective_overlap_pct = (1.0 - effective_stride_ratio) * 100.0;

        spdlog::info("DSP Results & Signal Statistics:");
        spdlog::info("  DSP Time       : {:.5f} seconds ({} rows x {} bins = {} pts)",
                     dsp_elapsed, num_rows, num_bins, count_db);
        spdlog::info("  Actual Stride  : {} samples/row ({:.4f} ms/row, stride ratio {:.3f}, overlap {:+.1f}%)",
                     actual_step_size, step_ms, effective_stride_ratio, effective_overlap_pct);
        spdlog::info("  Plot Coverage  : Duration: {:.3f}s, Freq Span: {:.6f} MHz (Center: {:.6f} MHz, {:.2f} Hz/bin)",
                     total_duration_sec, fs, z_center, freq_res_hz);
        spdlog::info("  Signal Power   : Min: {:.2f} dB, Max: {:.2f} dB, Mean: {:.2f} dB, Median Floor: {:.2f} dB (Dynamic Range: {:.2f} dB)",
                     actual_min_db, actual_max_db, mean_db, noise_floor_db, actual_max_db - actual_min_db);
        spdlog::info("  Plot dB Range  : {:.2f} to {:.2f} dB (Span: {:.2f} dB) [Min: {}, Max: {}]",
                     final_min_db, final_max_db, final_max_db - final_min_db,
                     (min_db == 1.0 ? "Auto (Mean - 15 dB)" : "Manual"),
                     (max_db == 1.0 ? "Auto (Peak)" : "Manual"));
        spdlog::info("==================================================================");

        std::vector<std::pair<int, int>> detected_peak_cells;
        std::vector<std::pair<int, int>> detected_outline_cells;
        if ((peak_detection || visualize_peaks) && !spectrogram.empty()) {
            spdlog::stopwatch sw_peaks;
            double threshold_db = noise_floor_db + peak_threshold;
            double step_dur_s = (file_sr > 0.0) ? (static_cast<double>(actual_step_size) / file_sr) : 0.0;
            double bin_bw_mhz = (num_bins > 0) ? (fs / static_cast<double>(num_bins)) : 0.0;
            double freq_min_mhz = z_center - (fs / 2.0);

            std::string peaks_csv_file = output_name + "_peaks.csv";
            std::string outlines_csv_file = output_name + "_outlines.csv";
            std::ofstream out_peaks_csv;
            if (peak_detection) {
                out_peaks_csv.open(peaks_csv_file);
                out_peaks_csv << "Row,Bin,Time_s,Duration_s,Freq_MHz,Bandwidth_MHz,Power_dB,SNR_dB\n";
            }

            // Grid marking above-threshold cells connected to local peaks for outline region merging
            std::vector<uint8_t> active_grid(num_rows * num_bins, 0);

            for (size_t r = 0; r < num_rows; ++r) {
                double time_s = result.original_start_time + r * step_dur_s;
                for (size_t c = 1; c + 1 < num_bins; ++c) {
                    double val = spectrogram[r][c];
                    if (val > threshold_db) {
                        if (val > spectrogram[r][c - 1] && val > spectrogram[r][c + 1]) {
                            detected_peak_cells.emplace_back(static_cast<int>(r), static_cast<int>(c));
                            if (peak_detection) {
                                double freq_mhz = freq_min_mhz + (c + 0.5) * bin_bw_mhz;
                                out_peaks_csv << r << "," << c << ","
                                              << std::fixed << std::setprecision(4) << time_s << ","
                                              << std::setprecision(4) << step_dur_s << ","
                                              << std::setprecision(6) << freq_mhz << ","
                                              << std::setprecision(6) << bin_bw_mhz << ","
                                              << std::setprecision(2) << val << ","
                                              << std::setprecision(2) << (val - noise_floor_db) << "\n";
                            }
                            // Expand horizontally across contiguous bins above threshold in this row
                            size_t c_left = c;
                            while (c_left > 0 && spectrogram[r][c_left - 1] > threshold_db) {
                                --c_left;
                            }
                            size_t c_right = c;
                            while (c_right + 1 < num_bins && spectrogram[r][c_right + 1] > threshold_db) {
                                ++c_right;
                            }
                            for (size_t cb = c_left; cb <= c_right; ++cb) {
                                active_grid[r * num_bins + cb] = 1;
                            }
                        }
                    }
                }
            }
            if (peak_detection) {
                out_peaks_csv.close();
            }

            for (size_t r = 0; r < num_rows; ++r) {
                for (size_t c = 0; c < num_bins; ++c) {
                    if (active_grid[r * num_bins + c]) {
                        detected_outline_cells.emplace_back(static_cast<int>(r), static_cast<int>(c));
                    }
                }
            }

            // Connected-component clustering matching the visual box neighborhood in outline mode
            int min_half_span = std::max(std::max(1, box_size) + 1, std::min(width, height) / 200);
            int approx_plot_w = std::max(1, width - 95 * text_scale);
            int approx_plot_h = std::max(1, height - 58 * text_scale);
            int link_r = std::max(1, static_cast<int>(std::ceil((2.0 * min_half_span * num_rows) / approx_plot_h)));
            int link_c = std::max(1, static_cast<int>(std::ceil((2.0 * min_half_span * num_bins) / approx_plot_w)));

            std::vector<int> visited(num_rows * num_bins, 0);
            size_t num_outlines = 0;
            std::ofstream out_outlines_csv;
            if (peak_detection) {
                out_outlines_csv.open(outlines_csv_file);
                out_outlines_csv << "Outline_ID,Start_Time_s,End_Time_s,Duration_s,Center_Freq_MHz,Mean_Freq_MHz,Min_Freq_MHz,Max_Freq_MHz,Min_BW_MHz,Max_BW_MHz,Avg_BW_MHz,Freq_Drift_MHz,Peak_Power_dB,Mean_Power_dB,Peak_SNR_dB,Mean_SNR_dB,Cell_Count,Active_Rows,Duty_Cycle\n";
            }

            std::vector<std::pair<int, int>> queue;
            for (size_t r0 = 0; r0 < num_rows; ++r0) {
                for (size_t c0 = 0; c0 < num_bins; ++c0) {
                    size_t idx0 = r0 * num_bins + c0;
                    if (!active_grid[idx0] || visited[idx0]) continue;

                    ++num_outlines;
                    queue.clear();
                    queue.emplace_back(static_cast<int>(r0), static_cast<int>(c0));
                    visited[idx0] = static_cast<int>(num_outlines);

                    int r_min = static_cast<int>(r0), r_max = static_cast<int>(r0);
                    int c_min_all = static_cast<int>(c0), c_max_all = static_cast<int>(c0);
                    double peak_power_db = -1e9;
                    double sum_power_db = 0.0;
                    double sum_lin_power = 0.0;
                    double sum_weighted_freq = 0.0;

                    size_t head = 0;
                    while (head < queue.size()) {
                        auto [cr, cc] = queue[head++];
                        if (cr < r_min) r_min = cr;
                        if (cr > r_max) r_max = cr;
                        if (cc < c_min_all) c_min_all = cc;
                        if (cc > c_max_all) c_max_all = cc;

                        double val = spectrogram[cr][cc];
                        if (val > peak_power_db) peak_power_db = val;
                        sum_power_db += val;
                        double lin_p = std::pow(10.0, val / 10.0);
                        sum_lin_power += lin_p;
                        double cell_f = freq_min_mhz + (cc + 0.5) * bin_bw_mhz;
                        sum_weighted_freq += lin_p * cell_f;

                        int nr_start = std::max(0, cr - link_r);
                        int nr_end = std::min(static_cast<int>(num_rows) - 1, cr + link_r);
                        int nc_start = std::max(0, cc - link_c);
                        int nc_end = std::min(static_cast<int>(num_bins) - 1, cc + link_c);
                        for (int nr = nr_start; nr <= nr_end; ++nr) {
                            for (int nc = nc_start; nc <= nc_end; ++nc) {
                                size_t n_idx = static_cast<size_t>(nr) * num_bins + static_cast<size_t>(nc);
                                if (active_grid[n_idx] && !visited[n_idx]) {
                                    visited[n_idx] = static_cast<int>(num_outlines);
                                    queue.emplace_back(nr, nc);
                                }
                            }
                        }
                    }

                    if (peak_detection) {
                        int span_rows = r_max - r_min + 1;
                        std::vector<int> row_cmin(span_rows, static_cast<int>(num_bins));
                        std::vector<int> row_cmax(span_rows, -1);
                        for (const auto& [cr, cc] : queue) {
                            int ri = cr - r_min;
                            if (cc < row_cmin[ri]) row_cmin[ri] = cc;
                            if (cc > row_cmax[ri]) row_cmax[ri] = cc;
                        }

                        double min_bw_mhz = 1e9;
                        double max_bw_mhz = 0.0;
                        double sum_bw_mhz = 0.0;
                        int active_rows = 0;
                        double first_row_cf = 0.0;
                        double last_row_cf = 0.0;
                        bool has_first_cf = false;

                        for (int ri = 0; ri < span_rows; ++ri) {
                            if (row_cmax[ri] >= row_cmin[ri]) {
                                ++active_rows;
                                double r_bw = (row_cmax[ri] - row_cmin[ri] + 1) * bin_bw_mhz;
                                if (r_bw < min_bw_mhz) min_bw_mhz = r_bw;
                                if (r_bw > max_bw_mhz) max_bw_mhz = r_bw;
                                sum_bw_mhz += r_bw;

                                double r_cf = freq_min_mhz + ((row_cmin[ri] + row_cmax[ri] + 1) * 0.5) * bin_bw_mhz;
                                if (!has_first_cf) {
                                    first_row_cf = r_cf;
                                    has_first_cf = true;
                                }
                                last_row_cf = r_cf;
                            }
                        }
                        if (active_rows == 0) min_bw_mhz = 0.0;
                        double avg_bw_mhz = (active_rows > 0) ? (sum_bw_mhz / active_rows) : 0.0;
                        double start_t = result.original_start_time + r_min * step_dur_s;
                        double dur_s = span_rows * step_dur_s;
                        double end_t = start_t + dur_s;
                        double min_f_mhz = freq_min_mhz + c_min_all * bin_bw_mhz;
                        double max_f_mhz = freq_min_mhz + (c_max_all + 1) * bin_bw_mhz;
                        double center_f_mhz = 0.5 * (min_f_mhz + max_f_mhz);
                        double mean_f_mhz = (sum_lin_power > 0.0) ? (sum_weighted_freq / sum_lin_power) : center_f_mhz;
                        double freq_drift_mhz = last_row_cf - first_row_cf;
                        double mean_power_db = (!queue.empty()) ? (sum_power_db / queue.size()) : peak_power_db;
                        double duty_cycle = (span_rows > 0) ? (static_cast<double>(active_rows) / span_rows) : 1.0;

                        out_outlines_csv << num_outlines << ","
                                         << std::fixed << std::setprecision(4) << start_t << ","
                                         << std::setprecision(4) << end_t << ","
                                         << std::setprecision(4) << dur_s << ","
                                         << std::setprecision(6) << center_f_mhz << ","
                                         << std::setprecision(6) << mean_f_mhz << ","
                                         << std::setprecision(6) << min_f_mhz << ","
                                         << std::setprecision(6) << max_f_mhz << ","
                                         << std::setprecision(6) << min_bw_mhz << ","
                                         << std::setprecision(6) << max_bw_mhz << ","
                                         << std::setprecision(6) << avg_bw_mhz << ","
                                         << std::setprecision(6) << freq_drift_mhz << ","
                                         << std::setprecision(2) << peak_power_db << ","
                                         << std::setprecision(2) << mean_power_db << ","
                                         << std::setprecision(2) << (peak_power_db - noise_floor_db) << ","
                                         << std::setprecision(2) << (mean_power_db - noise_floor_db) << ","
                                         << queue.size() << ","
                                         << active_rows << ","
                                         << std::setprecision(3) << duty_cycle << "\n";
                    }
                }
            }
            if (peak_detection) {
                out_outlines_csv.close();
                spdlog::info("Peak Detection : {} peak cells ({} active bins) merged into {} energy outlines above +{:.1f} dB (Floor: {:.2f} dB) in {:.5f}s",
                             detected_peak_cells.size(), detected_outline_cells.size(), num_outlines, peak_threshold, noise_floor_db, sw_peaks.elapsed().count());
                spdlog::info("  Cell Box CSV : {} ({})", peaks_csv_file, format_byte_size(get_file_size_bytes(peaks_csv_file)));
                spdlog::info("  Outlines CSV : {} ({})", outlines_csv_file, format_byte_size(get_file_size_bytes(outlines_csv_file)));
            } else {
                spdlog::info("Peak Detection : {} peak cells ({} active bins) merged into {} energy outlines above +{:.1f} dB (Floor: {:.2f} dB) in {:.5f}s (visualizing on waterfall)",
                             detected_peak_cells.size(), detected_outline_cells.size(), num_outlines, peak_threshold, noise_floor_db, sw_peaks.elapsed().count());
            }
        }

        auto log_output_metrics = [&](const std::string& label, const std::string& filepath, double render_sec) {
            size_t out_bytes = get_file_size_bytes(filepath);
            double ratio = (out_bytes > 0) ? (static_cast<double>(raw_rgb_bytes) / static_cast<double>(out_bytes)) : 0.0;
            double savings_pct = (raw_rgb_bytes > 0 && out_bytes <= raw_rgb_bytes)
                ? ((1.0 - static_cast<double>(out_bytes) / static_cast<double>(raw_rgb_bytes)) * 100.0)
                : 0.0;
            double bpp = (width > 0 && height > 0)
                ? ((static_cast<double>(out_bytes) * 8.0) / (static_cast<double>(width) * static_cast<double>(height)))
                : 0.0;
            spdlog::info("{} Output Summary:", label);
            spdlog::info("  Output File    : {}", filepath);
            spdlog::info("  Image Format   : {} ({}x{} px, --image-quality {})", fmt_upper, width, height, image_quality);
            spdlog::info("  Output Size    : {} (Raw RGB24: {})", format_byte_size(out_bytes), format_byte_size(raw_rgb_bytes));
            spdlog::info("  Compression    : {:.2f}:1 ({:.1f}% reduction, {:.2f} bits/pixel)", ratio, savings_pct, bpp);
            spdlog::info("  Render Time    : {:.5f} seconds", render_sec);
            spdlog::info("------------------------------------------------------------------");
        };

        if (plot_waterfall) {
            spdlog::stopwatch sw_plot;
            std::string wfile = (opt_output->count() > 0 && !plot_fft) ? output_name : output_name + "_waterfall." + out_format;
            if (opt_output->count() > 0 && wfile.find('.') == std::string::npos) {
                wfile += "." + out_format;
            }
            spdlog::info("Rendering Fast Waterfall [{}] ({}x{}) (dB: {:.1f} to {:.1f})...",
                         fmt_upper, width, height, final_min_db, final_max_db);
            
            char start_time_buf[64];
            if (timecode > 0.0) {
                double abs_time = timecode + start_time;
                time_t unix_time = static_cast<time_t>(abs_time - 631152000.0);
                struct tm tm_buf;
                struct tm* tm_info = gmtime_r(&unix_time, &tm_buf);
                if (tm_info) {
                    int millis = static_cast<int>(std::round((abs_time - std::floor(abs_time)) * 1000.0));
                    if (millis == 1000) {
                        millis = 0;
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
            const std::vector<std::pair<int, int>> empty_peaks;
            const auto& cells_to_draw = (visualize_mode == "outline") ? detected_outline_cells : detected_peak_cells;
            PlotGenerator::generate_fast_waterfall(
                spectrogram, wfile, width, height, colormap,
                final_min_db, final_max_db,
                z_center, fs, std::string(start_time_buf), total_duration_sec,
                draw_grid, draw_labels, out_format, x_ticks, y_ticks, title,
                image_quality, png_compression, font_path,
                box_start_time, box_duration, box_center_freq, box_bw, box_color,
                box_alpha, font_size, theme,
                visualize_peaks ? cells_to_draw : empty_peaks,
                visualize_mode, box_size);
            log_output_metrics("Waterfall", wfile, sw_plot.elapsed().count());
        }
        
        if (plot_fft && !spectrogram.empty()) {
            spdlog::stopwatch sw_plot;
            std::string ffile = (opt_output->count() > 0 && !plot_waterfall) ? output_name : output_name + "_fft." + out_format;
            if (opt_output->count() > 0 && ffile.find('.') == std::string::npos) {
                ffile += "." + out_format;
            }
            spdlog::info("Rendering Fast FFT Plot [{}] ({}x{}) (dB: {:.1f} to {:.1f})...",
                         fmt_upper, width, height, final_min_db, final_max_db);
            PlotGenerator::generate_fast_fft_plot(
                freq_bins, first_frame_mag, ffile, width, height,
                final_min_db, final_max_db, z_center, fs,
                draw_grid, draw_labels, out_format, x_ticks, y_ticks, title,
                image_quality, png_compression, colormap, font_path,
                font_size, theme, fft_fill, fft_color, fft_fill_opacity);
            log_output_metrics("FFT", ffile, sw_plot.elapsed().count());
        }

        spdlog::info("Done! Total elapsed time: {:.5f} seconds.", sw_total.elapsed().count());
        
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }

    return 0;
}
