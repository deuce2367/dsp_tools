#include "plot_generator.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "colormap.hpp"
#include "font8x8_basic.h"

// Helper to set a pixel
static void set_pixel(std::vector<unsigned char>& pixels, int width, int height, int x, int y, RGB color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int idx = (y * width + x) * 3;
        pixels[idx] = color.r;
        pixels[idx + 1] = color.g;
        pixels[idx + 2] = color.b;
    }
}

// Bresenham's line algorithm
static void draw_line(std::vector<unsigned char>& pixels, int width, int height, int x0, int y0, int x1, int y1, RGB color) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        set_pixel(pixels, width, height, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Draw a single character using font8x8
static void draw_char(std::vector<unsigned char>& pixels, int width, int height, int x, int y, char c, RGB color, int scale = 1) {
    if (c < 0 || c > 127) return;
    unsigned char* bitmap = font8x8_basic[static_cast<int>(c)];
    for (int r = 0; r < 8; ++r) {
        for (int c_bit = 0; c_bit < 8; ++c_bit) {
            if (bitmap[r] & (1 << c_bit)) {
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        set_pixel(pixels, width, height, x + c_bit * scale + sx, y + r * scale + sy, color);
                    }
                }
            }
        }
    }
}

// Draw text string
static void draw_text(std::vector<unsigned char>& pixels, int width, int height, int x, int y, const std::string& text, RGB color, int scale = 1) {
    int curr_x = x;
    for (char c : text) {
        draw_char(pixels, width, height, curr_x, y, c, color, scale);
        curr_x += 8 * scale;
    }
}

static void save_image(const std::string& filename, int width, int height, const std::vector<unsigned char>& pixels, const std::string& format) {
    if (format == "jpg" || format == "jpeg") {
        stbi_write_jpg(filename.c_str(), width, height, 3, pixels.data(), 90);
    } else {
        stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
    }
}

static void draw_axes_and_grid(std::vector<unsigned char>& pixels, int full_width, int full_height, 
                               int plot_x, int plot_y, int plot_w, int plot_h,
                               double center_freq_mhz, double bandwidth_mhz, 
                               const std::string& start_time_iso, double total_duration_sec,
                               bool draw_grid, bool draw_labels,
                               bool is_waterfall, double max_db, double min_db,
                               int num_x_ticks, int num_y_ticks,
                               const std::string& colormap = "",
                               const std::string& title = "") {
    RGB axis_color = {200, 200, 200};
    RGB grid_color = {80, 80, 80};
    
    // Draw Grid and Box
    draw_line(pixels, full_width, full_height, plot_x, plot_y, plot_x + plot_w - 1, plot_y, axis_color);
    draw_line(pixels, full_width, full_height, plot_x, plot_y + plot_h - 1, plot_x + plot_w - 1, plot_y + plot_h - 1, axis_color);
    draw_line(pixels, full_width, full_height, plot_x, plot_y, plot_x, plot_y + plot_h - 1, axis_color);
    draw_line(pixels, full_width, full_height, plot_x + plot_w - 1, plot_y, plot_x + plot_w - 1, plot_y + plot_h - 1, axis_color);

    if (draw_grid) {
        for (int i = 1; i < num_x_ticks; ++i) {
            int x = plot_x + plot_w * i / num_x_ticks;
            draw_line(pixels, full_width, full_height, x, plot_y, x, plot_y + plot_h - 1, grid_color);
        }
        for (int i = 1; i < num_y_ticks; ++i) {
            int y = plot_y + plot_h * i / num_y_ticks;
            draw_line(pixels, full_width, full_height, plot_x, y, plot_x + plot_w - 1, y, grid_color);
        }
    }

    // Draw Labels
    if (draw_labels) {
        int text_scale = std::max(1, full_width / 800);
        
        // X-axis: Frequencies
        if (bandwidth_mhz > 0.0) {
            double start_f = center_freq_mhz - bandwidth_mhz / 2.0;
            double step_f = bandwidth_mhz / num_x_ticks;
            
            for (int i = 0; i <= num_x_ticks; ++i) {
                double f = start_f + step_f * i;
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << f << " MHz";
                
                int x_pos = plot_x + (plot_w * i) / num_x_ticks;
                int text_width = ss.str().length() * 8 * text_scale;
                
                // Adjust x_pos to not go out of bounds
                if (i == 0) x_pos += 2;
                else if (i == num_x_ticks) x_pos -= text_width + 2;
                else x_pos -= text_width / 2;
                
                int y_pos = plot_y + plot_h + 10;
                draw_text(pixels, full_width, full_height, x_pos, y_pos, ss.str(), axis_color, text_scale);
            }
        }

        // Y-axis: Time or dB
        if (is_waterfall) {
            if (!start_time_iso.empty()) {
                std::string time_str = start_time_iso;
                size_t t_pos = time_str.find('T');
                if (t_pos != std::string::npos) {
                    std::string date_str = time_str.substr(0, t_pos);
                    std::string time_only_str = time_str.substr(t_pos + 1);
                    draw_text(pixels, full_width, full_height, 5, 10, date_str, axis_color, text_scale);
                    draw_text(pixels, full_width, full_height, 5, 10 + 10 * text_scale, time_only_str, axis_color, text_scale);
                } else {
                    draw_text(pixels, full_width, full_height, 5, 10, time_str, axis_color, text_scale);
                }
            } else {
                draw_text(pixels, full_width, full_height, 5, 10, "0.00s", axis_color, text_scale);
            }
            
            if (total_duration_sec > 0.0) {
                double step_t = total_duration_sec / num_y_ticks;
                for (int i = 1; i <= num_y_ticks; ++i) {
                    double t = step_t * i;
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(2) << "+" << t << "s";
                    
                    int y_pos = plot_y + (plot_h * i) / num_y_ticks - 4 * text_scale;
                    
                    // Align left
                    draw_text(pixels, full_width, full_height, 5, y_pos, ss.str(), axis_color, text_scale);
                }
            }
        } else {
            std::ostringstream ss_max, ss_min;
            ss_max << std::fixed << std::setprecision(0) << max_db << " dB";
            ss_min << std::fixed << std::setprecision(0) << min_db << " dB";
            draw_text(pixels, full_width, full_height, 5, plot_y, ss_max.str(), axis_color, text_scale);
            draw_text(pixels, full_width, full_height, 5, plot_y + plot_h - 10 * text_scale, ss_min.str(), axis_color, text_scale);
        }
        
        // Draw Legend (Colorbar)
        if (!colormap.empty() && plot_w > 0 && plot_h > 0) {
            int leg_w = 10 * text_scale;
            int leg_x = plot_x + plot_w + 10;
            int leg_y = plot_y;
            int leg_h = plot_h;
            
            for (int y = 0; y < leg_h; ++y) {
                float norm_val = 1.0f - static_cast<float>(y) / leg_h;
                RGB c;
                if (colormap == "inferno") c = Colormap::get_inferno(norm_val);
                else if (colormap == "plasma") c = Colormap::get_plasma(norm_val);
                else if (colormap == "magma") c = Colormap::get_magma(norm_val);
                else if (colormap == "coolwarm") c = Colormap::get_coolwarm(norm_val);
                else if (colormap == "turbo") c = Colormap::get_turbo(norm_val);
                else if (colormap == "jet") c = Colormap::get_jet(norm_val);
                else c = Colormap::get_viridis(norm_val);
                draw_line(pixels, full_width, full_height, leg_x, leg_y + y, leg_x + leg_w, leg_y + y, c);
            }
            
            // Draw box around legend
            draw_line(pixels, full_width, full_height, leg_x, leg_y, leg_x + leg_w, leg_y, axis_color);
            draw_line(pixels, full_width, full_height, leg_x, leg_y + leg_h, leg_x + leg_w, leg_y + leg_h, axis_color);
            draw_line(pixels, full_width, full_height, leg_x, leg_y, leg_x, leg_y + leg_h, axis_color);
            draw_line(pixels, full_width, full_height, leg_x + leg_w, leg_y, leg_x + leg_w, leg_y + leg_h, axis_color);
            
            std::ostringstream ss_max, ss_mid, ss_min;
            ss_max << std::fixed << std::setprecision(0) << max_db;
            ss_mid << std::fixed << std::setprecision(0) << (max_db + min_db) / 2.0;
            ss_min << std::fixed << std::setprecision(0) << min_db;
            
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5, leg_y, ss_max.str(), axis_color, text_scale);
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5, leg_y + leg_h / 2 - 4 * text_scale, ss_mid.str(), axis_color, text_scale);
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5, leg_y + leg_h - 10 * text_scale, ss_min.str(), axis_color, text_scale);
            
            // Draw 'dB' at the top of the colorbar
            draw_text(pixels, full_width, full_height, leg_x, leg_y - 12 * text_scale, "dB", axis_color, text_scale);
        }
        
        // Draw Title
        if (!title.empty()) {
            int text_width = title.length() * 8 * text_scale;
            int title_x = plot_x + plot_w / 2 - text_width / 2;
            int title_y = 10; // Top
            draw_text(pixels, full_width, full_height, title_x, title_y, title, {255, 255, 255}, text_scale);
        }
    }
}

void PlotGenerator::generate_fast_waterfall(const std::vector<std::vector<double>>& spectrogram_db,
                                            const std::string& output_filename,
                                            int out_width, int out_height,
                                            const std::string& colormap,
                                            double min_db, double max_db,
                                            double center_freq_mhz, double bandwidth_mhz,
                                            const std::string& start_time_iso, double total_duration_sec,
                                            bool draw_grid, bool draw_labels,
                                            const std::string& out_format,
                                            int num_x_ticks, int num_y_ticks,
                                            const std::string& title) {
    if (spectrogram_db.empty() || out_width <= 0 || out_height <= 0) return;
    
    int data_time_steps = spectrogram_db.size();
    int data_freq_bins = spectrogram_db[0].size();
    
    std::vector<unsigned char> pixels(out_width * out_height * 3, 0); // Initialize to black
    
    int margin_left = draw_labels ? 60 : 0;
    int margin_bottom = draw_labels ? 40 : 0;
    int margin_top = draw_labels ? 30 : 0;
    int margin_right = draw_labels ? 60 : 0;
    
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;
    
    for (int y = 0; y < plot_h; ++y) {
        int data_y = y * data_time_steps / plot_h;
        for (int x = 0; x < plot_w; ++x) {
            int data_x = x * data_freq_bins / plot_w;
            
            double db_val = spectrogram_db[data_y][data_x];
            RGB color;
            if (db_val <= min_db) {
                color = {0, 0, 0}; // Black background
            } else {
                float norm_val = static_cast<float>((db_val - min_db) / (max_db - min_db));
                norm_val = std::clamp(norm_val, 0.0f, 1.0f);
                
                if (colormap == "inferno") color = Colormap::get_inferno(norm_val);
                else if (colormap == "plasma") color = Colormap::get_plasma(norm_val);
                else if (colormap == "magma") color = Colormap::get_magma(norm_val);
                else if (colormap == "coolwarm") color = Colormap::get_coolwarm(norm_val);
                else if (colormap == "turbo") color = Colormap::get_turbo(norm_val);
                else if (colormap == "jet") color = Colormap::get_jet(norm_val);
                else color = Colormap::get_viridis(norm_val);
            }
            set_pixel(pixels, out_width, out_height, plot_x + x, plot_y + y, color);
        }
    }
    
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, start_time_iso, total_duration_sec, draw_grid, draw_labels, 
                       true, max_db, min_db, num_x_ticks, num_y_ticks, colormap, title);
    save_image(output_filename, out_width, out_height, pixels, out_format);
}

void PlotGenerator::generate_fast_fft_plot(const std::vector<double>& frequency_bins,
                                           const std::vector<double>& magnitude_db,
                                           const std::string& output_filename,
                                           int out_width, int out_height,
                                           double min_db, double max_db,
                                           double center_freq_mhz, double bandwidth_mhz,
                                           bool draw_grid, bool draw_labels,
                                           const std::string& out_format,
                                           int num_x_ticks, int num_y_ticks,
                                           const std::string& title) {
    if (magnitude_db.empty() || out_width <= 0 || out_height <= 0) return;

    // Dark background
    std::vector<unsigned char> pixels(out_width * out_height * 3, 10);
    
    int margin_left = draw_labels ? 60 : 0;
    int margin_bottom = draw_labels ? 40 : 0;
    int margin_top = draw_labels ? 30 : 0;
    int margin_right = draw_labels ? 20 : 0;
    
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;
    
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, "", 0.0, draw_grid, draw_labels, 
                       false, max_db, min_db, num_x_ticks, num_y_ticks, "", title);

    RGB line_color = {50, 150, 255}; // Nice light blue

    int prev_x = -1;
    int prev_y = -1;
    int data_bins = magnitude_db.size();
    
    for (int x = 0; x < plot_w; ++x) {
        int bin_idx = x * data_bins / plot_w;
        double db_val = magnitude_db[bin_idx];
        
        // Map dB to y
        float norm_val = static_cast<float>((db_val - min_db) / (max_db - min_db));
        norm_val = std::clamp(norm_val, 0.0f, 1.0f);
        int y = plot_h - 1 - static_cast<int>(norm_val * (plot_h - 1));
        
        if (x > 0) {
            draw_line(pixels, out_width, out_height, plot_x + prev_x, plot_y + prev_y, plot_x + x, plot_y + y, line_color);
        }
        prev_x = x;
        prev_y = y;
    }

    save_image(output_filename, out_width, out_height, pixels, out_format);
}
