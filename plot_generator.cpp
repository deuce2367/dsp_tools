#include "plot_generator.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "embedded_font.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include "colormap.hpp"

// Helper to parse color strings
static RGB parse_color(const std::string& color_str, RGB default_color) {
    if (color_str.empty()) return default_color;
    
    std::string s = color_str;
    // Lowercase it
    for (char& c : s) c = std::tolower(c);
    
    if (s == "red") return {255, 0, 0};
    if (s == "green") return {0, 255, 0};
    if (s == "blue") return {0, 0, 255};
    if (s == "yellow") return {255, 255, 0};
    if (s == "cyan") return {0, 255, 255};
    if (s == "magenta") return {255, 0, 255};
    if (s == "white") return {255, 255, 255};
    if (s == "black") return {0, 0, 0};
    
    // Hex code (#FF0000 or FF0000)
    if (s[0] == '#') s = s.substr(1);
    if (s.length() == 6 && s.find_first_not_of("0123456789abcdef") == std::string::npos) {
        int r, g, b;
        std::stringstream ss;
        ss << std::hex << s.substr(0, 2); ss >> r; ss.clear();
        ss << std::hex << s.substr(2, 2); ss >> g; ss.clear();
        ss << std::hex << s.substr(4, 2); ss >> b;
        return {static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b)};
    }
    
    // RGB comma separated (255, 0, 0)
    std::stringstream ss(s);
    std::string token;
    std::vector<int> vals;
    while (std::getline(ss, token, ',')) {
        try {
            vals.push_back(std::stoi(token));
        } catch(...) {}
    }
    if (vals.size() >= 3) {
        return {static_cast<unsigned char>(vals[0]), static_cast<unsigned char>(vals[1]), static_cast<unsigned char>(vals[2])};
    }
    
    return default_color;
}

// Helper to set a pixel
static void set_pixel(std::vector<unsigned char>& pixels, int width, int height, int x, int y, RGB color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int idx = (y * width + x) * 3;
        pixels[idx] = color.r;
        pixels[idx + 1] = color.g;
        pixels[idx + 2] = color.b;
    }
}

// Helper to blend a pixel with alpha
static void blend_pixel(std::vector<unsigned char>& pixels, int width, int height, int x, int y, RGB color, float alpha) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        int idx = (y * width + x) * 3;
        pixels[idx] = static_cast<unsigned char>(pixels[idx] * (1.0f - alpha) + color.r * alpha);
        pixels[idx + 1] = static_cast<unsigned char>(pixels[idx + 1] * (1.0f - alpha) + color.g * alpha);
        pixels[idx + 2] = static_cast<unsigned char>(pixels[idx + 2] * (1.0f - alpha) + color.b * alpha);
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


// TrueType Font caching
struct FontAtlas {
    bool loaded = false;
    std::string path = "";
    int scale = 1;
    stbtt_bakedchar cdata[96]; // ASCII 32..126
    std::vector<unsigned char> bitmap;
    int tex_w = 512;
    int tex_h = 512;
    float pixel_height;
};

static FontAtlas& get_font_atlas(const std::string& font_path, int scale) {
    static FontAtlas atlas;
    if (atlas.loaded && atlas.path == font_path && atlas.scale == scale) {
        return atlas;
    }
    
    atlas.loaded = false;
    atlas.path = font_path;
    atlas.scale = scale;
    
    std::vector<unsigned char> ttf_buffer;
    if (!font_path.empty()) {
        FILE* f = fopen(font_path.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            ttf_buffer.resize(size);
            if (fread(ttf_buffer.data(), 1, size, f)) {
                // Return value intentionally ignored
            }
            fclose(f);
        }
    }
    
    const unsigned char* ttf_data = nullptr;
    if (!ttf_buffer.empty()) {
        ttf_data = ttf_buffer.data();
    } else {
        ttf_data = DejaVuSansMono_ttf; // Fallback to embedded high-quality font
    }
    
    atlas.pixel_height = 12.0f * scale; // 12px base font size
    
    // Scale texture size based on scale
    atlas.tex_w = 512 * scale;
    atlas.tex_h = 512 * scale;
    atlas.bitmap.resize(atlas.tex_w * atlas.tex_h);
    
    int ret = stbtt_BakeFontBitmap(ttf_data, 0, atlas.pixel_height, atlas.bitmap.data(), atlas.tex_w, atlas.tex_h, 32, 96, atlas.cdata);
    if (ret > 0) {
        atlas.loaded = true;
    }
    return atlas;
}

static int get_text_width(const std::string& text, int scale, const std::string& font_path) {
    FontAtlas& atlas = get_font_atlas(font_path, scale);
    if (atlas.loaded) {
        float xpos = 0;
        float ypos = 0;
        for (char c : text) {
            if (c >= 32 && c < 128) {
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(atlas.cdata, atlas.tex_w, atlas.tex_h, c - 32, &xpos, &ypos, &q, 1);
            }
        }
        return static_cast<int>(xpos);
    } else {
        return text.length() * 8 * scale;
    }
}

// Draw text onto image
static void draw_text(std::vector<unsigned char>& pixels, int width, int height, int x, int y, const std::string& text, RGB color, int scale = 1, const std::string& font_path = "") {
    FontAtlas& atlas = get_font_atlas(font_path, scale);
    
    if (atlas.loaded) {
        float xpos = x;
        float ypos = y + atlas.pixel_height * 0.8f; // Adjust baseline
        
        for (char c : text) {
            if (c >= 32 && c < 128) {
                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(atlas.cdata, atlas.tex_w, atlas.tex_h, c - 32, &xpos, &ypos, &q, 1); // 1 = opengl coordinates
                
                int q_x0 = static_cast<int>(q.x0);
                int q_x1 = static_cast<int>(q.x1);
                int q_y0 = static_cast<int>(q.y0);
                int q_y1 = static_cast<int>(q.y1);
                
                for (int py = q_y0; py < q_y1; ++py) {
                    for (int px = q_x0; px < q_x1; ++px) {
                        int src_x = static_cast<int>(q.s0 * atlas.tex_w + (px - q_x0) * (q.s1 - q.s0) * atlas.tex_w / (q_x1 - q_x0));
                        int src_y = static_cast<int>(q.t0 * atlas.tex_h + (py - q_y0) * (q.t1 - q.t0) * atlas.tex_h / (q_y1 - q_y0));
                        
                        if (src_x >= 0 && src_x < atlas.tex_w && src_y >= 0 && src_y < atlas.tex_h) {
                            unsigned char alpha = atlas.bitmap[src_y * atlas.tex_w + src_x];
                            if (alpha > 0) {
                                blend_pixel(pixels, width, height, px, py, color, alpha / 255.0f);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void save_image(const std::string& filename, int width, int height, const std::vector<unsigned char>& pixels, const std::string& format, int jpeg_quality, int png_compression) {
    if (format == "jpg" || format == "jpeg") {
        stbi_write_jpg(filename.c_str(), width, height, 3, pixels.data(), jpeg_quality);
    } else {
        stbi_write_png_compression_level = png_compression;
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
                               const std::string& title = "",
                               const std::string& font_path = "") {
    RGB axis_color = {200, 200, 200};
    
    // Draw Grid and Box
    draw_line(pixels, full_width, full_height, plot_x, plot_y, plot_x + plot_w - 1, plot_y, axis_color);
    draw_line(pixels, full_width, full_height, plot_x, plot_y + plot_h - 1, plot_x + plot_w - 1, plot_y + plot_h - 1, axis_color);
    draw_line(pixels, full_width, full_height, plot_x, plot_y, plot_x, plot_y + plot_h - 1, axis_color);
    draw_line(pixels, full_width, full_height, plot_x + plot_w - 1, plot_y, plot_x + plot_w - 1, plot_y + plot_h - 1, axis_color);

    if (draw_grid) {
        for (int i = 1; i < num_x_ticks; ++i) {
            int x = plot_x + plot_w * i / num_x_ticks;
            for (int y = plot_y; y < plot_y + plot_h; ++y) {
                blend_pixel(pixels, full_width, full_height, x, y, {255, 255, 255}, 0.2f);
            }
        }
        for (int i = 1; i < num_y_ticks; ++i) {
            int y = plot_y + plot_h * i / num_y_ticks;
            for (int x = plot_x; x < plot_x + plot_w; ++x) {
                blend_pixel(pixels, full_width, full_height, x, y, {255, 255, 255}, 0.2f);
            }
        }
    }

    // Draw Labels
    if (draw_labels) {
        int text_scale = std::max(1, std::min(full_width, full_height) / 800);
        
        // X-axis: Frequencies
        if (bandwidth_mhz > 0.0) {
            if (num_x_ticks % 2 != 0) num_x_ticks++; // Force even so center is a tick
            
            double start_f = center_freq_mhz - bandwidth_mhz / 2.0;
            double step_f = bandwidth_mhz / num_x_ticks;
            
            int last_label_end_x = -1;
            for (int i = 0; i <= num_x_ticks; ++i) {
                double f = start_f + step_f * i;
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(2) << f;
                
                int x_pos = plot_x + (plot_w * i) / num_x_ticks;
                int text_width = get_text_width(ss.str(), text_scale, font_path);
                int draw_x_pos = x_pos - text_width / 2;
                
                // Draw tick mark
                draw_line(pixels, full_width, full_height, x_pos, plot_y + plot_h, x_pos, plot_y + plot_h + text_scale, axis_color);
                
                // Prevent overlaps
                if (i != 0 && i != num_x_ticks) {
                    if (draw_x_pos < last_label_end_x + 5 * text_scale) continue;
                    
                    std::ostringstream ss_last;
                    ss_last << std::fixed << std::setprecision(2) << start_f + step_f * num_x_ticks;
                    int last_width = get_text_width(ss_last.str(), text_scale, font_path);
                    int final_label_x = plot_x + plot_w - last_width / 2;
                    if (draw_x_pos + text_width + 5 * text_scale > final_label_x) continue;
                }
                
                int y_pos = plot_y + plot_h + 2 * text_scale;
                draw_text(pixels, full_width, full_height, draw_x_pos, y_pos, ss.str(), axis_color, text_scale, font_path);
                last_label_end_x = draw_x_pos + text_width;
            }
            
            std::string x_axis_title = "Frequency (MHz)";
            int title_scale = std::max(1, text_scale - 1);
            int title_w = get_text_width(x_axis_title, title_scale, font_path);
            int title_x = plot_x + plot_w / 2 - title_w / 2;
            int title_y = plot_y + plot_h + 15 * text_scale;
            draw_text(pixels, full_width, full_height, title_x, title_y, x_axis_title, axis_color, title_scale, font_path);
        }

        // Y-axis: Time or dB
        if (is_waterfall) {
            if (!start_time_iso.empty()) {
                std::string time_str = start_time_iso;
                size_t t_pos = time_str.find('T');
                if (t_pos != std::string::npos) {
                    std::string date_str = time_str.substr(0, t_pos);
                    std::string time_only_str = time_str.substr(t_pos + 1);
                    int date_w = get_text_width(date_str, text_scale, font_path);
                    int time_w = get_text_width(time_only_str, text_scale, font_path);
                    int time_x = std::max(2 * text_scale, plot_x - time_w - 3 * text_scale);
                    int date_x = time_x + (time_w / 2) - (date_w / 2);
                    draw_text(pixels, full_width, full_height, date_x, plot_y - 24 * text_scale, date_str, axis_color, text_scale, font_path);
                    draw_text(pixels, full_width, full_height, time_x, plot_y - 12 * text_scale, time_only_str, axis_color, text_scale, font_path);
                } else {
                    int text_w = get_text_width(time_str, text_scale, font_path);
                    int text_x = std::max(2 * text_scale, plot_x - text_w - 3 * text_scale);
                    draw_text(pixels, full_width, full_height, text_x, plot_y - 12 * text_scale, time_str, axis_color, text_scale, font_path);
                }
            } else {
                std::string s0 = "0.00s";
                int text_w = get_text_width(s0, text_scale, font_path);
                int text_x = std::max(2 * text_scale, plot_x - text_w - 3 * text_scale);
                draw_text(pixels, full_width, full_height, text_x, plot_y - 12 * text_scale, s0, axis_color, text_scale, font_path);
            }
            
            if (total_duration_sec > 0.0) {
                double step_t = total_duration_sec / num_y_ticks;
                for (int i = 1; i <= num_y_ticks; ++i) {
                    double t = step_t * i;
                    std::ostringstream ss;
                    ss << std::fixed << std::setprecision(2) << "+" << t << "s";
                    
                    int y_pos = plot_y + (plot_h * i) / num_y_ticks;
                    
                    // Draw tick mark
                    draw_line(pixels, full_width, full_height, plot_x - text_scale, y_pos, plot_x, y_pos, axis_color);
                    
                    // Omit the bottom-most label to prevent collision with the X-axis label
                    if (i < num_y_ticks) {
                        int text_width = get_text_width(ss.str(), text_scale, font_path);
                        draw_text(pixels, full_width, full_height, plot_x - text_width - 3 * text_scale, y_pos - 6 * text_scale, ss.str(), axis_color, text_scale, font_path);
                    }
                }
            }
        } else {
            std::ostringstream ss_max, ss_min;
            ss_max << std::fixed << std::setprecision(0) << max_db << " dB";
            ss_min << std::fixed << std::setprecision(0) << min_db << " dB";
            
            draw_line(pixels, full_width, full_height, plot_x - text_scale, plot_y, plot_x, plot_y, axis_color);
            draw_line(pixels, full_width, full_height, plot_x - text_scale, plot_y + plot_h - 1, plot_x, plot_y + plot_h - 1, axis_color);
            
            int text_width_max = get_text_width(ss_max.str(), text_scale, font_path);
            int text_width_min = get_text_width(ss_min.str(), text_scale, font_path);
            
            draw_text(pixels, full_width, full_height, plot_x - text_width_max - 3 * text_scale, plot_y - 6 * text_scale, ss_max.str(), axis_color, text_scale, font_path);
            draw_text(pixels, full_width, full_height, plot_x - text_width_min - 3 * text_scale, plot_y + plot_h - 6 * text_scale, ss_min.str(), axis_color, text_scale, font_path);
        }
        
        // Draw Legend (Colorbar)
        if (!colormap.empty() && plot_w > 0 && plot_h > 0) {
            int leg_w = 10 * text_scale;
            int leg_x = plot_x + plot_w + 8 * text_scale;
            int leg_y = plot_y;
            int leg_h = plot_h;
            
            for (int y = 0; y < leg_h; ++y) {
                float norm_val = 1.0f - static_cast<float>(y) / leg_h;
                RGB c;
                if (colormap == "electric") c = Colormap::get_electric(norm_val);
                else if (colormap == "gqrx") c = Colormap::get_gqrx(norm_val);
                else if (colormap == "websdr") c = Colormap::get_websdr(norm_val);
                else if (colormap == "pablo") c = Colormap::get_pablo(norm_val);
                else if (colormap == "turbo") c = Colormap::get_turbo(norm_val);
                else if (colormap == "frog") c = Colormap::get_frog(norm_val);
                else if (colormap == "grape") c = Colormap::get_grape(norm_val);
                else if (colormap == "jet") c = Colormap::get_jet(norm_val);
                else c = Colormap::get_turbo(norm_val);
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
            
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5 * text_scale, leg_y, ss_max.str(), axis_color, text_scale, font_path);
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5 * text_scale, leg_y + leg_h / 2 - 4 * text_scale, ss_mid.str(), axis_color, text_scale, font_path);
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5 * text_scale, leg_y + leg_h - 10 * text_scale, ss_min.str(), axis_color, text_scale, font_path);
            
            // Draw 'dB' at the top of the colorbar
            draw_text(pixels, full_width, full_height, leg_x, leg_y - 12 * text_scale, "dB", axis_color, text_scale, font_path);
        }
        
        // Draw Title
        if (!title.empty()) {
            int title_scale = text_scale + 1; // Make it larger
            int text_width = get_text_width(title, title_scale, font_path);
            
            // If title overruns, scale it down to fit
            if (text_width > plot_w && title_scale > 1) {
                title_scale = text_scale;
                text_width = get_text_width(title, title_scale, font_path);
            }
            
            int title_x = plot_x + plot_w / 2 - text_width / 2;
            int title_y = (plot_y - 12 * title_scale) / 2; // Vertically center in top margin
            draw_text(pixels, full_width, full_height, title_x, title_y, title, {255, 255, 255}, title_scale, font_path);
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
                                            const std::string& title,
                                            int jpeg_quality,
                                            int png_compression,
                                            const std::string& font_path,
                                            double box_start_time, double box_duration,
                                            double box_center_freq, double box_bw,
                                            const std::string& box_color) {
    if (spectrogram_db.empty() || out_width <= 0 || out_height <= 0) return;
    
    int data_time_steps = spectrogram_db.size();
    int data_freq_bins = spectrogram_db[0].size();
    
    std::vector<unsigned char> pixels(out_width * out_height * 3, 0); // Initialize to black
    
    int text_scale = std::max(1, std::min(out_width, out_height) / 800);
    int margin_left = draw_labels ? 50 * text_scale : 0;
    int margin_bottom = draw_labels ? 28 * text_scale : 0;
    int margin_top = draw_labels ? 30 * text_scale : 0;
    int margin_right = draw_labels ? (!colormap.empty() ? 45 * text_scale : 25 * text_scale) : 0;
    
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
                
                if (colormap == "electric") color = Colormap::get_electric(norm_val);
                else if (colormap == "gqrx") color = Colormap::get_gqrx(norm_val);
                else if (colormap == "websdr") color = Colormap::get_websdr(norm_val);
                else if (colormap == "pablo") color = Colormap::get_pablo(norm_val);
                else if (colormap == "turbo") color = Colormap::get_turbo(norm_val);
                else if (colormap == "frog") color = Colormap::get_frog(norm_val);
                else if (colormap == "grape") color = Colormap::get_grape(norm_val);
                else if (colormap == "jet") color = Colormap::get_jet(norm_val);
                else color = Colormap::get_turbo(norm_val);
            }
            set_pixel(pixels, out_width, out_height, plot_x + x, plot_y + y, color);
        }
    }
    
    // Draw optional box overlay
    if (box_start_time >= 0 && box_duration > 0 && box_bw > 0) {
        int start_y = plot_y + static_cast<int>((box_start_time / total_duration_sec) * plot_h);
        int end_y = plot_y + static_cast<int>(((box_start_time + box_duration) / total_duration_sec) * plot_h);
        
        double freq_min = center_freq_mhz - bandwidth_mhz / 2.0;
        double box_freq_min = box_center_freq - box_bw / 2.0;
        double box_freq_max = box_center_freq + box_bw / 2.0;
        
        int start_x = plot_x + static_cast<int>((box_freq_min - freq_min) / bandwidth_mhz * plot_w);
        int end_x = plot_x + static_cast<int>((box_freq_max - freq_min) / bandwidth_mhz * plot_w);
        
        // Clamp to plot area
        start_x = std::clamp(start_x, plot_x, plot_x + plot_w - 1);
        end_x = std::clamp(end_x, plot_x, plot_x + plot_w - 1);
        start_y = std::clamp(start_y, plot_y, plot_y + plot_h - 1);
        end_y = std::clamp(end_y, plot_y, plot_y + plot_h - 1);
        
        RGB color = {255, 0, 0}; // default red
        if (box_color == "green") color = {0, 255, 0};
        else if (box_color == "blue") color = {0, 100, 255};
        else if (box_color == "yellow") color = {255, 255, 0};
        else if (box_color == "white") color = {255, 255, 255};
        else if (box_color == "cyan") color = {0, 255, 255};
        else if (box_color == "magenta") color = {255, 0, 255};
        else if (box_color == "black") color = {0, 0, 0};
        
        int outline_thickness = std::max(1, out_width / 500);
        
        for (int y = start_y; y <= end_y; ++y) {
            for (int x = start_x; x <= end_x; ++x) {
                bool is_edge = (x < start_x + outline_thickness || x > end_x - outline_thickness || 
                                y < start_y + outline_thickness || y > end_y - outline_thickness);
                
                size_t idx = (y * out_width + x) * 3;
                if (is_edge) {
                    pixels[idx] = color.r;
                    pixels[idx + 1] = color.g;
                    pixels[idx + 2] = color.b;
                } else {
                    // Mild transparency (e.g. blend 30% color with 70% original)
                    pixels[idx] = static_cast<unsigned char>(pixels[idx] * 0.7 + color.r * 0.3);
                    pixels[idx + 1] = static_cast<unsigned char>(pixels[idx + 1] * 0.7 + color.g * 0.3);
                    pixels[idx + 2] = static_cast<unsigned char>(pixels[idx + 2] * 0.7 + color.b * 0.3);
                }
            }
        }
    }
    
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, start_time_iso, total_duration_sec, draw_grid, draw_labels, 
                       true, max_db, min_db, num_x_ticks, num_y_ticks, colormap, title, font_path);
    save_image(output_filename, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
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
                                           const std::string& title,
                                           int jpeg_quality,
                                           int png_compression,
                                           const std::string& colormap_name,
                                           const std::string& font_path) {
    if (magnitude_db.empty() || out_width <= 0 || out_height <= 0) return;

    // Dark background
    std::vector<unsigned char> pixels(out_width * out_height * 3, 10);
    
    int text_scale = std::max(1, std::min(out_width, out_height) / 800);
    int margin_left = draw_labels ? 50 * text_scale : 0;
    int margin_bottom = draw_labels ? 28 * text_scale : 0;
    int margin_top = draw_labels ? 30 * text_scale : 0;
    int margin_right = draw_labels ? 25 * text_scale : 0;
    
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;

    // Use empty string for start_time_iso to avoid printing date/time on FFT
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, "", 0.0, draw_grid, draw_labels, 
                       false, max_db, min_db, num_x_ticks, num_y_ticks, "", title, font_path);

    // Helper to extract a color from the requested colormap
    auto get_cmap_color = [&](float norm) -> RGB {
        if (colormap_name == "electric") return Colormap::get_electric(norm);
        if (colormap_name == "gqrx") return Colormap::get_gqrx(norm);
        if (colormap_name == "websdr") return Colormap::get_websdr(norm);
        if (colormap_name == "pablo") return Colormap::get_pablo(norm);
        if (colormap_name == "turbo") return Colormap::get_turbo(norm);
        if (colormap_name == "frog") return Colormap::get_frog(norm);
        if (colormap_name == "grape") return Colormap::get_grape(norm);
        if (colormap_name == "jet") return Colormap::get_jet(norm);
        return Colormap::get_turbo(norm); // default
    };

    int prev_x = -1;
    int prev_y = -1;
    int data_bins = magnitude_db.size();
    
    for (int x = 0; x < plot_w; ++x) {
        int bin_idx = x * data_bins / plot_w;
        double db_val = magnitude_db[bin_idx];
        
        float norm_val = static_cast<float>((db_val - min_db) / (max_db - min_db));
        norm_val = std::clamp(norm_val, 0.0f, 1.0f);
        int y = plot_h - 1 - static_cast<int>(norm_val * (plot_h - 1));
        
        RGB current_color = get_cmap_color(norm_val);
        
        if (x > 0) {
            draw_line(pixels, out_width, out_height, plot_x + prev_x, plot_y + prev_y, plot_x + x, plot_y + y, current_color);
        }
        
        // Fill opacity with gradient
        for (int fy = y + 1; fy < plot_h; ++fy) {
            float norm_fy = 1.0f - static_cast<float>(fy) / (plot_h - 1);
            RGB fill_color = get_cmap_color(norm_fy);
            blend_pixel(pixels, out_width, out_height, plot_x + x, plot_y + fy, fill_color, 0.5f);
        }
        
        prev_x = x;
        prev_y = y;
    }

    save_image(output_filename, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
}
