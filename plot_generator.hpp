#pragma once

#include <vector>
#include <string>

class PlotGenerator {
public:
    // Fast waterfall rendering using stb_image_write
    static void generate_fast_waterfall(const std::vector<std::vector<double>>& spectrogram_db,
                                        const std::string& output_filename,
                                        int out_width, int out_height,
                                        const std::string& colormap,
                                        double min_db, double max_db,
                                        double center_freq_mhz, double bandwidth_mhz,
                                        const std::string& start_time_iso, double total_duration_sec,
                                        bool draw_grid, bool draw_labels,
                                        const std::string& out_format,
                                        int num_x_ticks = 10, int num_y_ticks = 10,
                                        const std::string& title = "");

    // Fast FFT plot rendering
    static void generate_fast_fft_plot(const std::vector<double>& frequency_bins,
                                       const std::vector<double>& magnitude_db,
                                       const std::string& output_filename,
                                       int out_width, int out_height,
                                       double min_db, double max_db,
                                       double center_freq_mhz, double bandwidth_mhz,
                                       bool draw_grid, bool draw_labels,
                                       const std::string& out_format,
                                       int num_x_ticks = 10, int num_y_ticks = 10,
                                       const std::string& title = "");
};
