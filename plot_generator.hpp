#pragma once

#include <vector>
#include <string>
#include <utility>
#include <cstdint>

class PlotGenerator {
public:
    // Helper functions returning effective codec parameters for a given normalized image_quality (1-100)
    static int effective_png_compression(int image_quality, int png_compression = -1);
    static int effective_avif_quality(int image_quality);

    // Encode raw RGB pixels (3 bytes per pixel) to an image format in memory
    // Supported formats: "png", "jpg" / "jpeg", "webp", "avif"
    // image_quality is normalized from 1 to 100 across formats.
    static void encode_image_mem(const std::vector<uint8_t>& rgb_pixels,
                                 int width, int height,
                                 std::vector<uint8_t>& out_buffer,
                                 const std::string& out_format = "png",
                                 int image_quality = 50,
                                 int png_compression = -1);

    // Encode raw RGB pixels (3 bytes per pixel) to an image file on disk
    static void encode_image_file(const std::vector<uint8_t>& rgb_pixels,
                                  int width, int height,
                                  const std::string& output_filename,
                                  const std::string& out_format = "png",
                                  int image_quality = 50,
                                  int png_compression = -1);

    // Fast waterfall rendering
    static void generate_fast_waterfall(const std::vector<std::vector<double>>& spectrogram_db,
                                        const std::string& output_filename,
                                        int out_width, int out_height,
                                        const std::string& colormap,
                                        double min_db, double max_db,
                                        double center_freq_mhz, double bandwidth_mhz,
                                        const std::string& start_time_iso, double total_duration_sec,
                                        bool draw_grid = true, bool draw_labels = true,
                                        const std::string& out_format = "png",
                                        int num_x_ticks = 10, int num_y_ticks = 10,
                                        const std::string& title = "",
                                        int image_quality = 50,
                                        int png_compression = -1,
                                        const std::string& font_path = "",
                                        double box_start_time = -1.0, double box_duration = 0.0,
                                        double box_center_freq = 0.0, double box_bw = 0.0,
                                        const std::string& box_color = "red",
                                        float box_alpha = 0.3f,
                                        int font_size = 12,
                                        const std::string& theme = "dark",
                                        const std::vector<std::pair<int, int>>& peak_cells = {});

    // Fast FFT plot rendering
    static void generate_fast_fft_plot(const std::vector<double>& frequency_bins,
                                       const std::vector<double>& magnitude_db,
                                       const std::string& output_filename,
                                       int out_width, int out_height,
                                       double min_db, double max_db,
                                       double center_freq_mhz, double bandwidth_mhz,
                                       bool draw_grid = true, bool draw_labels = true,
                                       const std::string& out_format = "png",
                                       int num_x_ticks = 10, int num_y_ticks = 10,
                                       const std::string& title = "",
                                       int image_quality = 50,
                                       int png_compression = -1,
                                       const std::string& colormap_name = "jet",
                                       const std::string& font_path = "",
                                       int font_size = 12,
                                       const std::string& theme = "dark",
                                       const std::string& fill_mode = "gradient",
                                       const std::string& fill_color_str = "",
                                       float fill_opacity = 0.5f);

    // Fast waterfall rendering to memory buffer
    static void generate_fast_waterfall_mem(const std::vector<std::vector<double>>& spectrogram_db,
                                        std::vector<uint8_t>& out_buffer,
                                        int out_width, int out_height,
                                        const std::string& colormap,
                                        double min_db, double max_db,
                                        double center_freq_mhz, double bandwidth_mhz,
                                        const std::string& start_time_iso, double total_duration_sec,
                                        bool draw_grid = true, bool draw_labels = true,
                                        const std::string& out_format = "png",
                                        int num_x_ticks = 10, int num_y_ticks = 10,
                                        const std::string& title = "",
                                        int image_quality = 50,
                                        int png_compression = -1,
                                        const std::string& font_path = "",
                                        double box_start_time = -1.0, double box_duration = 0.0,
                                        double box_center_freq = 0.0, double box_bw = 0.0,
                                        const std::string& box_color = "red",
                                        const std::string& theme = "dark",
                                        float box_alpha = 0.3f,
                                        int font_size = 12,
                                        const std::vector<std::pair<int, int>>& peak_cells = {});

    // Fast FFT plot rendering to memory buffer
    static void generate_fast_fft_plot_mem(const std::vector<double>& frequency_bins,
                                       const std::vector<double>& magnitude_db,
                                       std::vector<uint8_t>& out_buffer,
                                       int out_width, int out_height,
                                       double min_db, double max_db,
                                       double center_freq_mhz, double bandwidth_mhz,
                                       bool draw_grid = true, bool draw_labels = true,
                                       const std::string& out_format = "png",
                                       int num_x_ticks = 10, int num_y_ticks = 10,
                                       const std::string& title = "",
                                       int image_quality = 50,
                                       int png_compression = -1,
                                       const std::string& colormap_name = "jet",
                                       const std::string& font_path = "",
                                       const std::string& theme = "dark",
                                       const std::string& fill_mode = "gradient",
                                       const std::string& fill_color_hex = "#00FF00",
                                       float fill_opacity = 0.5f,
                                       int font_size = 12);

    // Fast Time Domain Plot rendering to memory buffer
    static void generate_time_domain_plot_mem(const std::vector<float>& time_domain_data, bool is_complex,
                                       std::vector<uint8_t>& out_buffer,
                                       int out_width, int out_height,
                                       double min_val, double max_val,
                                       double start_time_sec, double duration_sec,
                                       bool draw_grid = true, bool draw_labels = true,
                                       const std::string& out_format = "png",
                                       int num_x_ticks = 10, int num_y_ticks = 10,
                                       const std::string& title = "",
                                       int image_quality = 50,
                                       int png_compression = -1,
                                       const std::string& font_path = "",
                                       const std::string& theme = "dark",
                                       const std::string& color_i_hex = "#00FF00",
                                       const std::string& color_q_hex = "#0088FF",
                                       int font_size = 12);

    // Fast Constellation Plot rendering to memory buffer
    static void generate_constellation_plot_mem(const std::vector<float>& iq_data,
                                       std::vector<uint8_t>& out_buffer,
                                       int out_width, int out_height,
                                       double min_val, double max_val,
                                       bool draw_grid = true, bool draw_labels = true,
                                       const std::string& out_format = "png",
                                       int num_x_ticks = 10, int num_y_ticks = 10,
                                       const std::string& title = "",
                                       int image_quality = 50,
                                       int png_compression = -1,
                                       const std::string& font_path = "",
                                       const std::string& theme = "dark",
                                       const std::string& color_hex = "#00FF00",
                                       int font_size = 12);

};
