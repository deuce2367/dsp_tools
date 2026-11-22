#include "plot_generator.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include "embedded_font.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <webp/encode.h>
#include <avif/avif.h>
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
    int base_font_size = 12;
    stbtt_bakedchar cdata[96]; // ASCII 32..126
    std::vector<unsigned char> bitmap;
    int tex_w = 512;
    int tex_h = 512;
    float pixel_height;
};

static FontAtlas& get_font_atlas(const std::string& font_path, int scale, int base_font_size = 12) {
    static FontAtlas atlas;
    int effective_base = (base_font_size > 0) ? base_font_size : 12;
    int effective_scale = std::max(1, scale);
    if (atlas.loaded && atlas.path == font_path && atlas.scale == effective_scale && atlas.base_font_size == effective_base) {
        return atlas;
    }
    
    atlas.loaded = false;
    atlas.path = font_path;
    atlas.scale = effective_scale;
    atlas.base_font_size = effective_base;
    
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
    
    atlas.pixel_height = static_cast<float>(effective_base * effective_scale);
    
    // Scale texture size based on pixel height relative to 12px base
    int tex_scale = std::max(1, (static_cast<int>(atlas.pixel_height) + 11) / 12);
    atlas.tex_w = 512 * tex_scale;
    atlas.tex_h = 512 * tex_scale;
    atlas.bitmap.resize(atlas.tex_w * atlas.tex_h);
    
    int ret = stbtt_BakeFontBitmap(ttf_data, 0, atlas.pixel_height, atlas.bitmap.data(), atlas.tex_w, atlas.tex_h, 32, 96, atlas.cdata);
    if (ret > 0) {
        atlas.loaded = true;
    }
    return atlas;
}

static int get_text_width(const std::string& text, int scale, const std::string& font_path, int base_font_size = 12) {
    FontAtlas& atlas = get_font_atlas(font_path, scale, base_font_size);
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
        int effective_base = (base_font_size > 0) ? base_font_size : 12;
        return static_cast<int>(text.length() * (effective_base * 2 / 3) * std::max(1, scale));
    }
}

// Draw text onto image
static void draw_text(std::vector<unsigned char>& pixels, int width, int height, int x, int y, const std::string& text, RGB color, int scale = 1, const std::string& font_path = "", int base_font_size = 12) {
    FontAtlas& atlas = get_font_atlas(font_path, scale, base_font_size);
    
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

static std::string normalize_image_format(const std::string& format, const std::string& filename = "") {
    std::string fmt = format;
    if (!fmt.empty() && fmt[0] == '.') fmt = fmt.substr(1);
    for (char& c : fmt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (fmt.empty() && !filename.empty()) {
        size_t dot = filename.find_last_of('.');
        if (dot != std::string::npos) {
            fmt = filename.substr(dot + 1);
            for (char& c : fmt) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (fmt == "jpeg") fmt = "jpg";
    if (fmt.empty()) fmt = "png";
    return fmt;
}

static int normalize_png_compression(int image_quality, int png_compression) {
    if (png_compression >= 0) {
        return std::clamp(png_compression, 0, 9);
    }
    int q = std::clamp(image_quality, 1, 100);
    return std::clamp(static_cast<int>(std::round((q - 1) * 9.0 / 99.0)), 0, 9);
}

static int normalize_avif_quality(int image_quality) {
    int q = std::clamp(image_quality, 1, 100);
    if (q <= 75) {
        return std::clamp(static_cast<int>(std::round(1.0 + (q - 1) * (49.0 / 74.0))), 1, 100);
    }
    return std::clamp(static_cast<int>(std::round(50.0 + (q - 75) * (35.0 / 25.0))), 1, 100);
}

static void stbi_write_mem_func(void *context, void *data, int size) {
    std::vector<uint8_t>* vec = static_cast<std::vector<uint8_t>*>(context);
    uint8_t* ptr = static_cast<uint8_t*>(data);
    vec->insert(vec->end(), ptr, ptr + size);
}

static void save_image_mem(std::vector<uint8_t>& out_buffer, int width, int height, const std::vector<unsigned char>& pixels, const std::string& format, int image_quality, int png_compression) {
    if (width <= 0 || height <= 0 || pixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 3) {
        throw std::invalid_argument("Invalid image dimensions or pixel buffer size");
    }
    std::string fmt = normalize_image_format(format);
    int q = std::clamp(image_quality, 1, 100);

    if (fmt == "jpg") {
        stbi_write_jpg_to_func(stbi_write_mem_func, &out_buffer, width, height, 3, pixels.data(), q);
    } else if (fmt == "webp") {
        uint8_t* webp_data = nullptr;
        size_t webp_size = WebPEncodeRGB(pixels.data(), width, height, width * 3, static_cast<float>(q), &webp_data);
        if (webp_size == 0 || webp_data == nullptr) {
            if (webp_data) WebPFree(webp_data);
            throw std::runtime_error("Failed to encode WebP image");
        }
        out_buffer.insert(out_buffer.end(), webp_data, webp_data + webp_size);
        WebPFree(webp_data);
    } else if (fmt == "avif") {
        avifImage* image = avifImageCreate(width, height, 8, AVIF_PIXEL_FORMAT_YUV420);
        if (!image) {
            throw std::runtime_error("Failed to create AVIF image");
        }
        avifRGBImage rgb;
        avifRGBImageSetDefaults(&rgb, image);
        rgb.format = AVIF_RGB_FORMAT_RGB;
        rgb.depth = 8;
        rgb.pixels = const_cast<uint8_t*>(pixels.data());
        rgb.rowBytes = static_cast<uint32_t>(width * 3);

        avifResult convertResult = avifImageRGBToYUV(image, &rgb);
        if (convertResult != AVIF_RESULT_OK) {
            avifImageDestroy(image);
            throw std::runtime_error(std::string("Failed to convert RGB to YUV for AVIF: ") + avifResultToString(convertResult));
        }

        avifEncoder* encoder = avifEncoderCreate();
        if (!encoder) {
            avifImageDestroy(image);
            throw std::runtime_error("Failed to create AVIF encoder");
        }
        int avif_q = normalize_avif_quality(q);
#if defined(AVIF_VERSION_MAJOR) && (AVIF_VERSION_MAJOR >= 1)
        encoder->quality = avif_q;
        encoder->qualityAlpha = AVIF_QUALITY_DEFAULT;
#else
        int quantizer = std::clamp(static_cast<int>(std::round((100 - avif_q) * 63.0 / 99.0)), 0, 63);
        encoder->minQuantizer = quantizer;
        encoder->maxQuantizer = quantizer;
#endif
        encoder->speed = 6;
        encoder->maxThreads = 4;

        avifRWData avifOutput = AVIF_DATA_EMPTY;
        avifResult encodeResult = avifEncoderWrite(encoder, image, &avifOutput);
        avifEncoderDestroy(encoder);
        avifImageDestroy(image);

        if (encodeResult != AVIF_RESULT_OK) {
            avifRWDataFree(&avifOutput);
            throw std::runtime_error(std::string("Failed to encode AVIF image: ") + avifResultToString(encodeResult));
        }

        out_buffer.insert(out_buffer.end(), avifOutput.data, avifOutput.data + avifOutput.size);
        avifRWDataFree(&avifOutput);
    } else if (fmt == "png") {
        stbi_write_png_compression_level = normalize_png_compression(q, png_compression);
        stbi_write_png_to_func(stbi_write_mem_func, &out_buffer, width, height, 3, pixels.data(), width * 3);
    } else {
        throw std::invalid_argument("Unsupported output image format: " + format);
    }
}

static void save_image(const std::string& filename, int width, int height, const std::vector<unsigned char>& pixels, const std::string& format, int image_quality, int png_compression) {
    std::string fmt = normalize_image_format(format, filename);
    int q = std::clamp(image_quality, 1, 100);

    if (fmt == "jpg") {
        stbi_write_jpg(filename.c_str(), width, height, 3, pixels.data(), q);
    } else if (fmt == "png") {
        stbi_write_png_compression_level = normalize_png_compression(q, png_compression);
        stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3);
    } else if (fmt == "webp" || fmt == "avif") {
        std::vector<uint8_t> out_buffer;
        save_image_mem(out_buffer, width, height, pixels, fmt, q, png_compression);
        std::ofstream out(filename, std::ios::binary);
        if (!out) {
            throw std::runtime_error("Failed to open output file for writing: " + filename);
        }
        out.write(reinterpret_cast<const char*>(out_buffer.data()), static_cast<std::streamsize>(out_buffer.size()));
    } else {
        throw std::invalid_argument("Unsupported output image format: " + format);
    }
}

int PlotGenerator::effective_png_compression(int image_quality, int png_compression) {
    return normalize_png_compression(image_quality, png_compression);
}

int PlotGenerator::effective_avif_quality(int image_quality) {
    return normalize_avif_quality(image_quality);
}

void PlotGenerator::encode_image_mem(const std::vector<uint8_t>& rgb_pixels,
                                     int width, int height,
                                     std::vector<uint8_t>& out_buffer,
                                     const std::string& out_format,
                                     int image_quality,
                                     int png_compression) {
    save_image_mem(out_buffer, width, height, rgb_pixels, out_format, image_quality, png_compression);
}

void PlotGenerator::encode_image_file(const std::vector<uint8_t>& rgb_pixels,
                                      int width, int height,
                                      const std::string& output_filename,
                                      const std::string& out_format,
                                      int image_quality,
                                      int png_compression) {
    save_image(output_filename, width, height, rgb_pixels, out_format, image_quality, png_compression);
}

static void compute_plot_margins(int out_width, int out_height, bool draw_labels, bool has_colorbar, int font_size,
                                 int& text_scale, int& margin_left, int& margin_bottom, int& margin_top, int& margin_right) {
    text_scale = std::max(1, std::min(out_width, out_height) / 800);
    if (!draw_labels) {
        margin_left = 0;
        margin_bottom = 0;
        margin_top = 0;
        margin_right = 0;
        return;
    }
    int effective_font = (font_size > 0) ? font_size : 12;
    if (effective_font == 12) {
        margin_left = 50 * text_scale;
        margin_bottom = 28 * text_scale;
        margin_top = 30 * text_scale;
        margin_right = has_colorbar ? (45 * text_scale) : (25 * text_scale);
    } else {
        float ratio = static_cast<float>(effective_font) / 12.0f;
        margin_left = std::max(24 * text_scale, static_cast<int>(std::round(50 * text_scale * ratio)));
        margin_bottom = std::max(16 * text_scale, static_cast<int>(std::round(28 * text_scale * ratio)));
        margin_top = std::max(18 * text_scale, static_cast<int>(std::round(30 * text_scale * ratio)));
        margin_right = has_colorbar
            ? std::max(24 * text_scale, static_cast<int>(std::round(45 * text_scale * ratio)))
            : std::max(14 * text_scale, static_cast<int>(std::round(25 * text_scale * ratio)));
        if (margin_left + margin_right > out_width * 2 / 3) {
            float clamp_x = (out_width * 2.0f / 3.0f) / static_cast<float>(margin_left + margin_right);
            margin_left = static_cast<int>(margin_left * clamp_x);
            margin_right = static_cast<int>(margin_right * clamp_x);
        }
        if (margin_top + margin_bottom > out_height * 2 / 3) {
            float clamp_y = (out_height * 2.0f / 3.0f) / static_cast<float>(margin_top + margin_bottom);
            margin_top = static_cast<int>(margin_top * clamp_y);
            margin_bottom = static_cast<int>(margin_bottom * clamp_y);
        }
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
                               const std::string& font_path = "",
                               const std::string& theme = "dark",
                               bool is_db = true,
                               const std::string& x_axis_title = "Frequency (MHz)",
                               int font_size = 12) {
    RGB axis_color = (theme == "light") ? RGB{30, 30, 30} : RGB{255, 255, 255};
    RGB grid_color = (theme == "light") ? RGB{0, 0, 0} : RGB{255, 255, 255};
    
    // Draw Grid and Box
    draw_line(pixels, full_width, full_height, plot_x, plot_y, plot_x + plot_w - 1, plot_y, axis_color);
    draw_line(pixels, full_width, full_height, plot_x, plot_y + plot_h - 1, plot_x + plot_w - 1, plot_y + plot_h - 1, axis_color);
    draw_line(pixels, full_width, full_height, plot_x, plot_y, plot_x, plot_y + plot_h - 1, axis_color);
    draw_line(pixels, full_width, full_height, plot_x + plot_w - 1, plot_y, plot_x + plot_w - 1, plot_y + plot_h - 1, axis_color);

    if (draw_grid) {
        for (int i = 1; i < num_x_ticks; ++i) {
            int x = plot_x + plot_w * i / num_x_ticks;
            for (int y = plot_y; y < plot_y + plot_h; ++y) {
                blend_pixel(pixels, full_width, full_height, x, y, grid_color, 0.2f);
            }
        }
        for (int i = 1; i < num_y_ticks; ++i) {
            int y = plot_y + plot_h * i / num_y_ticks;
            for (int x = plot_x; x < plot_x + plot_w; ++x) {
                blend_pixel(pixels, full_width, full_height, x, y, grid_color, 0.2f);
            }
        }
    }

    // Draw Labels
    if (draw_labels) {
        int text_scale = std::max(1, std::min(full_width, full_height) / 800);
        int effective_font = (font_size > 0) ? font_size : 12;
        int font_px = effective_font * text_scale;
        
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
                int text_width = get_text_width(ss.str(), text_scale, font_path, effective_font);
                int draw_x_pos = x_pos - text_width / 2;
                
                // Draw tick mark
                draw_line(pixels, full_width, full_height, x_pos, plot_y + plot_h, x_pos, plot_y + plot_h + text_scale, axis_color);
                
                // Prevent overlaps
                if (i != 0 && i != num_x_ticks) {
                    if (draw_x_pos < last_label_end_x + 5 * text_scale) continue;
                    
                    std::ostringstream ss_last;
                    ss_last << std::fixed << std::setprecision(2) << start_f + step_f * num_x_ticks;
                    int last_width = get_text_width(ss_last.str(), text_scale, font_path, effective_font);
                    int final_label_x = plot_x + plot_w - last_width / 2;
                    if (draw_x_pos + text_width + 5 * text_scale > final_label_x) continue;
                }
                
                int y_pos = plot_y + plot_h + 2 * text_scale;
                draw_text(pixels, full_width, full_height, draw_x_pos, y_pos, ss.str(), axis_color, text_scale, font_path, effective_font);
                last_label_end_x = draw_x_pos + text_width;
            }
            
            int title_scale = std::max(1, text_scale - 1);
            int title_w = get_text_width(x_axis_title, title_scale, font_path, effective_font);
            int title_x = plot_x + plot_w / 2 - title_w / 2;
            int title_y = plot_y + plot_h + font_px + 3 * text_scale;
            draw_text(pixels, full_width, full_height, title_x, title_y, x_axis_title, axis_color, title_scale, font_path, effective_font);
        }

        // Y-axis: Time or dB
        if (is_waterfall) {
            if (!start_time_iso.empty()) {
                std::string time_str = start_time_iso;
                size_t t_pos = time_str.find('T');
                if (t_pos != std::string::npos) {
                    std::string date_str = time_str.substr(0, t_pos);
                    std::string time_only_str = time_str.substr(t_pos + 1);
                    int date_w = get_text_width(date_str, text_scale, font_path, effective_font);
                    int time_w = get_text_width(time_only_str, text_scale, font_path, effective_font);
                    int time_x = std::max(2 * text_scale, plot_x - time_w - 3 * text_scale);
                    int date_x = time_x + (time_w / 2) - (date_w / 2);
                    draw_text(pixels, full_width, full_height, date_x, plot_y - 2 * font_px, date_str, axis_color, text_scale, font_path, effective_font);
                    draw_text(pixels, full_width, full_height, time_x, plot_y - font_px, time_only_str, axis_color, text_scale, font_path, effective_font);
                } else {
                    int text_w = get_text_width(time_str, text_scale, font_path, effective_font);
                    int text_x = std::max(2 * text_scale, plot_x - text_w - 3 * text_scale);
                    draw_text(pixels, full_width, full_height, text_x, plot_y - font_px, time_str, axis_color, text_scale, font_path, effective_font);
                }
            } else {
                std::string s0 = "0.00s";
                int text_w = get_text_width(s0, text_scale, font_path, effective_font);
                int text_x = std::max(2 * text_scale, plot_x - text_w - 3 * text_scale);
                draw_text(pixels, full_width, full_height, text_x, plot_y - font_px, s0, axis_color, text_scale, font_path, effective_font);
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
                        int text_width = get_text_width(ss.str(), text_scale, font_path, effective_font);
                        draw_text(pixels, full_width, full_height, plot_x - text_width - 3 * text_scale, y_pos - font_px / 2, ss.str(), axis_color, text_scale, font_path, effective_font);
                    }
                }
            }
        } else {
            double step_db = (max_db - min_db) / num_y_ticks;
            for (int i = 0; i <= num_y_ticks; ++i) {
                double db_val = max_db - (step_db * i);
                int y_pos = plot_y + (plot_h * i) / num_y_ticks;
                
                std::ostringstream ss;
                if (is_db) {
                    ss << std::fixed << std::setprecision(0) << db_val << " dB";
                } else {
                    ss << std::defaultfloat << std::setprecision(3) << db_val;
                }
                
                // Draw tick mark
                draw_line(pixels, full_width, full_height, plot_x - text_scale, y_pos, plot_x, y_pos, axis_color);
                
                int text_width = get_text_width(ss.str(), text_scale, font_path, effective_font);
                
                // Adjust label position to avoid clipping at the bottom
                int label_y = y_pos - font_px / 2;
                if (i == num_y_ticks) label_y = y_pos - font_px;
                
                draw_text(pixels, full_width, full_height, plot_x - text_width - 3 * text_scale, label_y, ss.str(), axis_color, text_scale, font_path, effective_font);
            }
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
                else if (colormap == "greyscale") c = Colormap::get_greyscale(norm_val);
                else if (colormap == "sunset") c = Colormap::get_sunset(norm_val);
                else if (colormap == "hot") c = Colormap::get_hot(norm_val);
                else if (colormap == "cold") c = Colormap::get_cold(norm_val);
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
            
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5 * text_scale, leg_y, ss_max.str(), axis_color, text_scale, font_path, effective_font);
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5 * text_scale, leg_y + leg_h / 2 - font_px / 3, ss_mid.str(), axis_color, text_scale, font_path, effective_font);
            draw_text(pixels, full_width, full_height, leg_x + leg_w + 5 * text_scale, leg_y + leg_h - (font_px - 2 * text_scale), ss_min.str(), axis_color, text_scale, font_path, effective_font);
            
            // Draw 'dB' at the top of the colorbar
            if (is_db) {
                draw_text(pixels, full_width, full_height, leg_x, leg_y - font_px, "dB", axis_color, text_scale, font_path, effective_font);
            }
        }
        
        // Draw Title
        if (!title.empty()) {
            int title_scale = text_scale + 1; // Make it larger
            int text_width = get_text_width(title, title_scale, font_path, effective_font);
            
            // If title overruns, scale it down to fit
            if (text_width > plot_w && title_scale > 1) {
                title_scale = text_scale;
                text_width = get_text_width(title, title_scale, font_path, effective_font);
            }
            
            int title_x = plot_x + plot_w / 2 - text_width / 2;
            int title_y = std::max(0, (plot_y - effective_font * title_scale) / 2); // Vertically center in top margin
            draw_text(pixels, full_width, full_height, title_x, title_y, title, axis_color, title_scale, font_path, effective_font);
        }
    }
}

static void draw_highlight_box(std::vector<unsigned char>& pixels, int out_width, int out_height,
                               int plot_x, int plot_y, int plot_w, int plot_h,
                               double center_freq_mhz, double bandwidth_mhz, double total_duration_sec,
                               double box_start_time, double box_duration,
                               double box_center_freq, double box_bw,
                               const std::string& box_color, float box_alpha,
                               int box_size = 1) {
    if (box_start_time >= 0 && box_duration > 0 && box_bw > 0 && total_duration_sec > 0.0 && bandwidth_mhz > 0.0) {
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
        
        RGB color = (box_color == "blue") ? RGB{0, 100, 255} : parse_color(box_color, {255, 0, 0});
        float alpha = std::clamp(box_alpha, 0.0f, 1.0f);
        float inv_alpha = 1.0f - alpha;
        
        int outline_thickness = std::max(1, box_size);
        
        for (int y = start_y; y <= end_y; ++y) {
            for (int x = start_x; x <= end_x; ++x) {
                bool is_edge = (x < start_x + outline_thickness || x > end_x - outline_thickness || 
                                y < start_y + outline_thickness || y > end_y - outline_thickness);
                
                size_t idx = (y * out_width + x) * 3;
                if (is_edge) {
                    pixels[idx] = color.r;
                    pixels[idx + 1] = color.g;
                    pixels[idx + 2] = color.b;
                } else if (alpha > 0.0f) {
                    pixels[idx] = static_cast<unsigned char>(pixels[idx] * inv_alpha + color.r * alpha);
                    pixels[idx + 1] = static_cast<unsigned char>(pixels[idx + 1] * inv_alpha + color.g * alpha);
                    pixels[idx + 2] = static_cast<unsigned char>(pixels[idx + 2] * inv_alpha + color.b * alpha);
                }
            }
        }
    }
}

static void draw_peak_cell_boxes(std::vector<unsigned char>& pixels, int out_width, int out_height,
                                 int plot_x, int plot_y, int plot_w, int plot_h,
                                 int data_time_steps, int data_freq_bins,
                                 const std::vector<std::pair<int, int>>& peak_cells,
                                 const std::string& box_color, float box_alpha,
                                 const std::string& visualize_mode = "cell",
                                 int box_size = 1) {
    if (peak_cells.empty() || plot_w <= 0 || plot_h <= 0 || data_time_steps <= 0 || data_freq_bins <= 0) return;

    RGB color = (box_color == "blue") ? RGB{0, 100, 255} : parse_color(box_color, {255, 255, 255});
    float alpha = std::clamp(box_alpha, 0.0f, 1.0f);
    float inv_alpha = 1.0f - alpha;

    std::string mode = visualize_mode;
    for (char& ch : mode) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bool outline_mode = (mode == "outline");

    int outline_thickness = std::max(1, box_size);

    // 0 = none, 1 = interior, 2 = border
    std::vector<uint8_t> mask(static_cast<size_t>(out_width) * static_cast<size_t>(out_height), 0);
    int min_half_span = std::max(outline_thickness + 1, std::min(out_width, out_height) / 200);

    for (const auto& cell : peak_cells) {
        int r = cell.first;
        int c = cell.second;
        if (r < 0 || r >= data_time_steps || c < 0 || c >= data_freq_bins) continue;

        int x0 = plot_x + (c * plot_w) / data_freq_bins;
        int x1 = plot_x + ((c + 1) * plot_w) / data_freq_bins;
        int y0 = plot_y + (r * plot_h) / data_time_steps;
        int y1 = plot_y + ((r + 1) * plot_h) / data_time_steps;

        if (x1 - x0 < 2 * min_half_span) {
            int cx = (x0 + x1) / 2;
            x0 = cx - min_half_span;
            x1 = cx + min_half_span;
        }
        if (y1 - y0 < 2 * min_half_span) {
            int cy = (y0 + y1) / 2;
            y0 = cy - min_half_span;
            y1 = cy + min_half_span;
        }

        x0 = std::clamp(x0, plot_x, plot_x + plot_w - 1);
        x1 = std::clamp(x1, plot_x, plot_x + plot_w - 1);
        y0 = std::clamp(y0, plot_y, plot_y + plot_h - 1);
        y1 = std::clamp(y1, plot_y, plot_y + plot_h - 1);

        if (outline_mode) {
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    size_t m_idx = static_cast<size_t>(y) * static_cast<size_t>(out_width) + static_cast<size_t>(x);
                    mask[m_idx] = 1;
                }
            }
        } else {
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    bool is_edge = (x < x0 + outline_thickness || x > x1 - outline_thickness ||
                                    y < y0 + outline_thickness || y > y1 - outline_thickness);
                    size_t m_idx = static_cast<size_t>(y) * static_cast<size_t>(out_width) + static_cast<size_t>(x);
                    if (is_edge) {
                        mask[m_idx] = 2;
                    } else if (mask[m_idx] == 0) {
                        mask[m_idx] = 1;
                    }
                }
            }
        }
    }

    if (outline_mode) {
        int max_dist_sq = outline_thickness * outline_thickness;
        for (int y = plot_y; y < plot_y + plot_h; ++y) {
            for (int x = plot_x; x < plot_x + plot_w; ++x) {
                size_t m_idx = static_cast<size_t>(y) * static_cast<size_t>(out_width) + static_cast<size_t>(x);
                if (mask[m_idx] == 0) continue;

                bool is_boundary = false;
                for (int dy = -outline_thickness; dy <= outline_thickness && !is_boundary; ++dy) {
                    int ny = y + dy;
                    for (int dx = -outline_thickness; dx <= outline_thickness; ++dx) {
                        if (dx * dx + dy * dy > max_dist_sq) continue;
                        int nx = x + dx;
                        if (ny < plot_y || ny >= plot_y + plot_h || nx < plot_x || nx >= plot_x + plot_w) {
                            is_boundary = true;
                            break;
                        }
                        size_t n_idx = static_cast<size_t>(ny) * static_cast<size_t>(out_width) + static_cast<size_t>(nx);
                        if (mask[n_idx] == 0) {
                            is_boundary = true;
                            break;
                        }
                    }
                }
                if (is_boundary) {
                    mask[m_idx] = 2;
                }
            }
        }
    }

    for (int y = plot_y; y < plot_y + plot_h; ++y) {
        for (int x = plot_x; x < plot_x + plot_w; ++x) {
            size_t m_idx = static_cast<size_t>(y) * static_cast<size_t>(out_width) + static_cast<size_t>(x);
            uint8_t state = mask[m_idx];
            if (state == 0) continue;
            size_t p_idx = m_idx * 3;
            if (state == 2) {
                pixels[p_idx] = color.r;
                pixels[p_idx + 1] = color.g;
                pixels[p_idx + 2] = color.b;
            } else if (state == 1 && alpha > 0.0f) {
                pixels[p_idx] = static_cast<unsigned char>(pixels[p_idx] * inv_alpha + color.r * alpha);
                pixels[p_idx + 1] = static_cast<unsigned char>(pixels[p_idx + 1] * inv_alpha + color.g * alpha);
                pixels[p_idx + 2] = static_cast<unsigned char>(pixels[p_idx + 2] * inv_alpha + color.b * alpha);
            }
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
                                            const std::string& box_color,
                                            float box_alpha,
                                            int font_size,
                                            const std::string& theme,
                                            const std::vector<std::pair<int, int>>& peak_cells,
                                            const std::string& visualize_mode,
                                            int box_size) {
    if (spectrogram_db.empty() || out_width <= 0 || out_height <= 0) return;
    
    int data_time_steps = spectrogram_db.size();
    int data_freq_bins = spectrogram_db[0].size();
    unsigned char bg = (theme == "light") ? 255 : 0;
    std::vector<unsigned char> pixels(out_width * out_height * 3, bg);
    
    int text_scale, margin_left, margin_bottom, margin_top, margin_right;
    compute_plot_margins(out_width, out_height, draw_labels, !colormap.empty(), font_size,
                         text_scale, margin_left, margin_bottom, margin_top, margin_right);
    
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
                else if (colormap == "greyscale") color = Colormap::get_greyscale(norm_val);
                else if (colormap == "sunset") color = Colormap::get_sunset(norm_val);
                else if (colormap == "hot") color = Colormap::get_hot(norm_val);
                else if (colormap == "cold") color = Colormap::get_cold(norm_val);
                else color = Colormap::get_turbo(norm_val);
            }
            set_pixel(pixels, out_width, out_height, plot_x + x, plot_y + y, color);
        }
    }
    
    draw_highlight_box(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, total_duration_sec,
                       box_start_time, box_duration, box_center_freq, box_bw, box_color, box_alpha, box_size);

    draw_peak_cell_boxes(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                         data_time_steps, data_freq_bins, peak_cells, box_color, box_alpha, visualize_mode, box_size);
    
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, start_time_iso, total_duration_sec, draw_grid, draw_labels, 
                       true, max_db, min_db, num_x_ticks, num_y_ticks, colormap, title, font_path, theme, true, "Frequency (MHz)", font_size);
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
                                           const std::string& font_path,
                                           int font_size,
                                           const std::string& theme,
                                           const std::string& fill_mode,
                                           const std::string& fill_color_str,
                                           float fill_opacity) {
    if (magnitude_db.empty() || out_width <= 0 || out_height <= 0) return;

    unsigned char bg = (theme == "light") ? 255 : 10;
    std::vector<unsigned char> pixels(out_width * out_height * 3, bg);
    
    int text_scale, margin_left, margin_bottom, margin_top, margin_right;
    compute_plot_margins(out_width, out_height, draw_labels, false, font_size,
                         text_scale, margin_left, margin_bottom, margin_top, margin_right);
    
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;

    // Use empty string for start_time_iso to avoid printing date/time on FFT
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, "", 0.0, draw_grid, draw_labels, 
                       false, max_db, min_db, num_x_ticks, num_y_ticks, "", title, font_path, theme, true, "Frequency (MHz)", font_size);

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
        if (colormap_name == "greyscale") return Colormap::get_greyscale(norm);
        if (colormap_name == "sunset") return Colormap::get_sunset(norm);
        if (colormap_name == "hot") return Colormap::get_hot(norm);
        if (colormap_name == "cold") return Colormap::get_cold(norm);
        return Colormap::get_turbo(norm); // default
    };

    bool has_custom_line = !fill_color_str.empty();
    RGB custom_line_color = parse_color(fill_color_str, get_cmap_color(0.75f));
    std::string mode_lower = fill_mode;
    for (char& c : mode_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool is_gradient_fill = (mode_lower.empty() || mode_lower == "gradient");
    bool is_no_fill = (mode_lower == "none" || fill_opacity <= 0.001f);
    RGB solid_fill_color = (mode_lower == "solid" || is_gradient_fill || is_no_fill)
        ? custom_line_color
        : parse_color(fill_mode, custom_line_color);
    float clamped_opacity = std::clamp(fill_opacity, 0.0f, 1.0f);

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
        RGB line_color = has_custom_line ? custom_line_color : (is_gradient_fill ? current_color : solid_fill_color);
        
        if (x > 0) {
            draw_line(pixels, out_width, out_height, plot_x + prev_x, plot_y + prev_y, plot_x + x, plot_y + y, line_color);
        }
        
        if (!is_no_fill) {
            for (int fy = y + 1; fy < plot_h; ++fy) {
                float norm_fy = 1.0f - static_cast<float>(fy) / (plot_h - 1);
                RGB fill_color = is_gradient_fill ? get_cmap_color(norm_fy) : solid_fill_color;
                blend_pixel(pixels, out_width, out_height, plot_x + x, plot_y + fy, fill_color, clamped_opacity);
            }
        }
        
        prev_x = x;
        prev_y = y;
    }

    save_image(output_filename, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
}

void PlotGenerator::generate_fast_waterfall_mem(const std::vector<std::vector<double>>& spectrogram_db,
                                            std::vector<uint8_t>& out_buffer,
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
                                            const std::string& box_color,
                                            const std::string& theme,
                                            float box_alpha,
                                            int font_size,
                                            const std::vector<std::pair<int, int>>& peak_cells,
                                            const std::string& visualize_mode,
                                            int box_size) {
    if (spectrogram_db.empty() || out_width <= 0 || out_height <= 0) return;
    
    int data_time_steps = spectrogram_db.size();
    int data_freq_bins = spectrogram_db[0].size();
    unsigned char bg = (theme == "light") ? 255 : 10;
    std::vector<unsigned char> pixels(out_width * out_height * 3, bg);
    
    int text_scale, margin_left, margin_bottom, margin_top, margin_right;
    compute_plot_margins(out_width, out_height, draw_labels, !colormap.empty(), font_size,
                         text_scale, margin_left, margin_bottom, margin_top, margin_right);
    
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
                else if (colormap == "greyscale") color = Colormap::get_greyscale(norm_val);
                else if (colormap == "sunset") color = Colormap::get_sunset(norm_val);
                else if (colormap == "hot") color = Colormap::get_hot(norm_val);
                else if (colormap == "cold") color = Colormap::get_cold(norm_val);
                else color = Colormap::get_turbo(norm_val);
            }
            set_pixel(pixels, out_width, out_height, plot_x + x, plot_y + y, color);
        }
    }
    
    draw_highlight_box(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, total_duration_sec,
                       box_start_time, box_duration, box_center_freq, box_bw, box_color, box_alpha, box_size);

    draw_peak_cell_boxes(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                         data_time_steps, data_freq_bins, peak_cells, box_color, box_alpha, visualize_mode, box_size);
    
    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, start_time_iso, total_duration_sec, draw_grid, draw_labels, 
                       true, max_db, min_db, num_x_ticks, num_y_ticks, colormap, title, font_path, theme, true, "Frequency (MHz)", font_size);
    save_image_mem(out_buffer, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
}

void PlotGenerator::generate_fast_fft_plot_mem(const std::vector<double>& frequency_bins,
                                           const std::vector<double>& magnitude_db,
                                           std::vector<uint8_t>& out_buffer,
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
                                           const std::string& font_path,
                                           const std::string& theme,
                                           const std::string& fill_mode,
                                           const std::string& fill_color_hex,
                                           float fill_opacity,
                                           int font_size) {
    if (magnitude_db.empty() || out_width <= 0 || out_height <= 0) return;
    unsigned char bg = (theme == "light") ? 255 : 10;
    std::vector<unsigned char> pixels(out_width * out_height * 3, bg);
    int text_scale, margin_left, margin_bottom, margin_top, margin_right;
    compute_plot_margins(out_width, out_height, draw_labels, false, font_size,
                         text_scale, margin_left, margin_bottom, margin_top, margin_right);
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;

    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       center_freq_mhz, bandwidth_mhz, "", 0.0, draw_grid, draw_labels, 
                       false, max_db, min_db, num_x_ticks, num_y_ticks, "", title, font_path, theme, true, "Frequency (MHz)", font_size);

    auto get_cmap_color = [&](float norm) -> RGB {
        if (colormap_name == "electric") return Colormap::get_electric(norm);
        if (colormap_name == "gqrx") return Colormap::get_gqrx(norm);
        if (colormap_name == "websdr") return Colormap::get_websdr(norm);
        if (colormap_name == "pablo") return Colormap::get_pablo(norm);
        if (colormap_name == "turbo") return Colormap::get_turbo(norm);
        if (colormap_name == "frog") return Colormap::get_frog(norm);
        if (colormap_name == "grape") return Colormap::get_grape(norm);
        if (colormap_name == "jet") return Colormap::get_jet(norm);
        if (colormap_name == "greyscale") return Colormap::get_greyscale(norm);
        if (colormap_name == "sunset") return Colormap::get_sunset(norm);
        if (colormap_name == "hot") return Colormap::get_hot(norm);
        if (colormap_name == "cold") return Colormap::get_cold(norm);
        return Colormap::get_turbo(norm); 
    };

    RGB custom_fill_color = parse_color(fill_color_hex, {0, 255, 0});
    float clamped_opacity = std::clamp(fill_opacity, 0.0f, 1.0f);

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
        RGB line_color = (fill_mode != "gradient") ? custom_fill_color : current_color;

        if (x > 0) {
            draw_line(pixels, out_width, out_height, plot_x + prev_x, plot_y + prev_y, plot_x + x, plot_y + y, line_color);
        }
        if (fill_mode != "none" && clamped_opacity > 0.001f) {
            for (int fy = y + 1; fy < plot_h; ++fy) {
                float norm_fy = 1.0f - static_cast<float>(fy) / (plot_h - 1);
                RGB fill_color = (fill_mode == "solid") ? custom_fill_color : get_cmap_color(norm_fy);
                blend_pixel(pixels, out_width, out_height, plot_x + x, plot_y + fy, fill_color, clamped_opacity);
            }
        }
        prev_x = x;
        prev_y = y;
    }
    save_image_mem(out_buffer, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
}

void PlotGenerator::generate_time_domain_plot_mem(const std::vector<float>& time_domain_data, bool is_complex,
                                           std::vector<uint8_t>& out_buffer,
                                           int out_width, int out_height,
                                           double min_val, double max_val,
                                           double start_time_sec, double duration_sec,
                                           bool draw_grid, bool draw_labels,
                                           const std::string& out_format,
                                           int num_x_ticks, int num_y_ticks,
                                           const std::string& title,
                                           int jpeg_quality,
                                           int png_compression,
                                           const std::string& font_path,
                                           const std::string& theme,
                                           const std::string& color_i_hex,
                                           const std::string& color_q_hex,
                                           int font_size) {
    if (time_domain_data.empty() || out_width <= 0 || out_height <= 0) return;
    unsigned char bg = (theme == "light") ? 255 : 10;
    std::vector<unsigned char> pixels(out_width * out_height * 3, bg);
    int effective_font = (font_size > 0) ? font_size : 12;
    int text_scale, margin_left, margin_bottom, margin_top, margin_right;
    compute_plot_margins(out_width, out_height, draw_labels, false, effective_font,
                         text_scale, margin_left, margin_bottom, margin_top, margin_right);
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;

    draw_axes_and_grid(pixels, out_width, out_height, plot_x, plot_y, plot_w, plot_h,
                       0.0, 0.0, "", 0.0, draw_grid, draw_labels, 
                       false, max_val, min_val, num_x_ticks, num_y_ticks, "", title, font_path, theme, false, "Time (seconds)", effective_font);

    if (draw_labels) {
        RGB axis_color = (theme == "light") ? RGB{30, 30, 30} : RGB{255, 255, 255};
        double step_t = duration_sec / num_x_ticks;
        int last_label_end_x = -1;
        for (int i = 0; i <= num_x_ticks; ++i) {
            double t = start_time_sec + step_t * i;
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << t << "s";
            
            int x_pos = plot_x + (plot_w * i) / num_x_ticks;
            int text_width = get_text_width(ss.str(), text_scale, font_path, effective_font);
            int draw_x_pos = x_pos - text_width / 2;
            
            draw_line(pixels, out_width, out_height, x_pos, plot_y + plot_h, x_pos, plot_y + plot_h + text_scale, axis_color);
            
            if (i != 0 && i != num_x_ticks) {
                if (draw_x_pos < last_label_end_x + 5 * text_scale) continue;
                std::ostringstream ss_last;
                ss_last << std::fixed << std::setprecision(2) << start_time_sec + duration_sec << "s";
                int last_width = get_text_width(ss_last.str(), text_scale, font_path, effective_font);
                int final_label_x = plot_x + plot_w - last_width / 2;
                if (draw_x_pos + text_width + 5 * text_scale > final_label_x) continue;
            }
            
            int y_pos = plot_y + plot_h + 2 * text_scale;
            draw_text(pixels, out_width, out_height, draw_x_pos, y_pos, ss.str(), axis_color, text_scale, font_path, effective_font);
            last_label_end_x = draw_x_pos + text_width;
        }
        int title_y = plot_y + plot_h + (effective_font * text_scale) + 3 * text_scale;
        std::string x_axis_title = "Time (seconds)";
        int title_scale = std::max(1, text_scale - 1);
        int title_w = get_text_width(x_axis_title, title_scale, font_path, effective_font);
        int title_x = plot_x + plot_w / 2 - title_w / 2;
        draw_text(pixels, out_width, out_height, title_x, title_y, x_axis_title, axis_color, title_scale, font_path, effective_font);
    }

    RGB color_i = parse_color(color_i_hex, {0, 255, 0});
    RGB color_q = parse_color(color_q_hex, {0, 136, 255});

    size_t floats_per_bucket = is_complex ? 4 : 2;
    int data_bins = time_domain_data.size() / floats_per_bucket;

    for (int x = 0; x < plot_w; ++x) {
        int bin_start = x * data_bins / plot_w;
        int bin_end = (x + 1) * data_bins / plot_w;
        if (bin_end > data_bins) bin_end = data_bins;
        if (bin_start >= data_bins) bin_start = data_bins - 1;
        
        if (is_complex) {
            float g_min_i = 1e9f, g_max_i = -1e9f;
            float g_min_q = 1e9f, g_max_q = -1e9f;
            for (int i = bin_start; i < bin_end; ++i) {
                float min_i = time_domain_data[i * 4 + 0];
                float min_q = time_domain_data[i * 4 + 1];
                float max_i = time_domain_data[i * 4 + 2];
                float max_q = time_domain_data[i * 4 + 3];
                if (min_i < g_min_i) g_min_i = min_i;
                if (max_i > g_max_i) g_max_i = max_i;
                if (min_q < g_min_q) g_min_q = min_q;
                if (max_q > g_max_q) g_max_q = max_q;
            }
            
            float norm_max_i = static_cast<float>((g_max_i - min_val) / (max_val - min_val));
            float norm_min_i = static_cast<float>((g_min_i - min_val) / (max_val - min_val));
            int y_max_i = plot_h - 1 - static_cast<int>(std::clamp(norm_max_i, 0.0f, 1.0f) * (plot_h - 1));
            int y_min_i = plot_h - 1 - static_cast<int>(std::clamp(norm_min_i, 0.0f, 1.0f) * (plot_h - 1));
            
            float norm_max_q = static_cast<float>((g_max_q - min_val) / (max_val - min_val));
            float norm_min_q = static_cast<float>((g_min_q - min_val) / (max_val - min_val));
            int y_max_q = plot_h - 1 - static_cast<int>(std::clamp(norm_max_q, 0.0f, 1.0f) * (plot_h - 1));
            int y_min_q = plot_h - 1 - static_cast<int>(std::clamp(norm_min_q, 0.0f, 1.0f) * (plot_h - 1));
            
            draw_line(pixels, out_width, out_height, plot_x + x, plot_y + y_max_i, plot_x + x, plot_y + y_min_i, color_i);
            draw_line(pixels, out_width, out_height, plot_x + x, plot_y + y_max_q, plot_x + x, plot_y + y_min_q, color_q);
        } else {
            float g_min_v = 1e9f, g_max_v = -1e9f;
            for (int i = bin_start; i < bin_end; ++i) {
                float min_v = time_domain_data[i * 2 + 0];
                float max_v = time_domain_data[i * 2 + 1];
                if (min_v < g_min_v) g_min_v = min_v;
                if (max_v > g_max_v) g_max_v = max_v;
            }
            
            float norm_max_v = static_cast<float>((g_max_v - min_val) / (max_val - min_val));
            float norm_min_v = static_cast<float>((g_min_v - min_val) / (max_val - min_val));
            int y_max_v = plot_h - 1 - static_cast<int>(std::clamp(norm_max_v, 0.0f, 1.0f) * (plot_h - 1));
            int y_min_v = plot_h - 1 - static_cast<int>(std::clamp(norm_min_v, 0.0f, 1.0f) * (plot_h - 1));
            
            draw_line(pixels, out_width, out_height, plot_x + x, plot_y + y_max_v, plot_x + x, plot_y + y_min_v, color_i);
        }
    }
    save_image_mem(out_buffer, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
}

void PlotGenerator::generate_constellation_plot_mem(const std::vector<float>& iq_data,
                                   std::vector<uint8_t>& out_buffer,
                                   int out_width, int out_height,
                                   double min_val, double max_val,
                                   bool draw_grid, bool draw_labels,
                                   const std::string& out_format,
                                   int num_x_ticks, int num_y_ticks,
                                   const std::string& title,
                                   int jpeg_quality, int png_compression,
                                   const std::string& font_path,
                                   const std::string& theme,
                                   const std::string& color_hex,
                                   int font_size) {
    RGB bg_color = (theme == "dark") ? RGB{0, 0, 0} : RGB{255, 255, 255};
    RGB axis_color = (theme == "dark") ? RGB{255, 255, 255} : RGB{50, 50, 50};
    RGB pt_color = parse_color(color_hex, {0, 255, 0});

    std::vector<uint8_t> pixels(out_width * out_height * 3, 0);
    for (size_t i = 0; i < pixels.size(); i += 3) {
        pixels[i] = bg_color.r; pixels[i+1] = bg_color.g; pixels[i+2] = bg_color.b;
    }

    int effective_font = (font_size > 0) ? font_size : 12;
    int text_scale = std::max(1, std::min(out_width, out_height) / 800);
    float font_ratio = static_cast<float>(effective_font) / 12.0f;
    int margin_left = static_cast<int>(std::round(60 * text_scale * font_ratio));
    int margin_bottom = static_cast<int>(std::round(40 * text_scale * font_ratio));
    int margin_top = static_cast<int>(std::round(30 * text_scale * font_ratio));
    int margin_right = static_cast<int>(std::round(30 * text_scale * font_ratio));

    if (!draw_labels) { margin_left = 10; margin_bottom = 10; margin_top = 10; margin_right = 10; }
    
    int plot_x = margin_left;
    int plot_y = margin_top;
    int plot_w = out_width - margin_left - margin_right;
    int plot_h = out_height - margin_top - margin_bottom;

    if (draw_grid) {
        // Draw center crosshairs if 0 is in range
        if (min_val <= 0.0 && max_val >= 0.0) {
            double range = max_val - min_val;
            if (range < 1e-6) range = 1e-6;
            int zero_x = plot_x + static_cast<int>(((0.0 - min_val) / range) * plot_w);
            int zero_y = plot_y + plot_h - 1 - static_cast<int>(((0.0 - min_val) / range) * plot_h);
            draw_line(pixels, out_width, out_height, zero_x, plot_y, zero_x, plot_y + plot_h, axis_color);
            draw_line(pixels, out_width, out_height, plot_x, zero_y, plot_x + plot_w, zero_y, axis_color);
        }
    }
    // Draw outer box
    draw_line(pixels, out_width, out_height, plot_x, plot_y, plot_x + plot_w, plot_y, axis_color);
    draw_line(pixels, out_width, out_height, plot_x, plot_y + plot_h, plot_x + plot_w, plot_y + plot_h, axis_color);
    draw_line(pixels, out_width, out_height, plot_x, plot_y, plot_x, plot_y + plot_h, axis_color);
    draw_line(pixels, out_width, out_height, plot_x + plot_w, plot_y, plot_x + plot_w, plot_y + plot_h, axis_color);

    if (draw_labels) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f", min_val);
        draw_text(pixels, out_width, out_height, plot_x, plot_y + plot_h + 5, buf, axis_color, text_scale, font_path, effective_font);
        std::snprintf(buf, sizeof(buf), "%.2f", max_val);
        draw_text(pixels, out_width, out_height, plot_x + plot_w - get_text_width(buf, text_scale, font_path, effective_font), plot_y + plot_h + 5, buf, axis_color, text_scale, font_path, effective_font);
        
        std::snprintf(buf, sizeof(buf), "%.2f", max_val);
        draw_text(pixels, out_width, out_height, 2, plot_y, buf, axis_color, text_scale, font_path, effective_font);
        std::snprintf(buf, sizeof(buf), "%.2f", min_val);
        draw_text(pixels, out_width, out_height, 2, plot_y + plot_h - effective_font * text_scale - 3, buf, axis_color, text_scale, font_path, effective_font);
    }

    // Draw Title
    if (draw_labels && !title.empty()) {
        int title_scale = std::max(1, text_scale);
        int title_w = get_text_width(title, title_scale, font_path, effective_font);
        draw_text(pixels, out_width, out_height, out_width / 2 - title_w / 2, 5 * text_scale, title, axis_color, title_scale, font_path, effective_font);
    }

    size_t num_pts = iq_data.size() / 2;
    double range = max_val - min_val;
    if (range < 1e-6) range = 1e-6;

    for (size_t i = 0; i < num_pts; ++i) {
        double real_val = iq_data[i * 2];
        double imag_val = iq_data[i * 2 + 1];

        if (real_val < min_val || real_val > max_val || imag_val < min_val || imag_val > max_val) continue;

        int x = static_cast<int>(((real_val - min_val) / range) * plot_w);
        int y = plot_h - 1 - static_cast<int>(((imag_val - min_val) / range) * plot_h);

        int px = plot_x + x;
        int py = plot_y + y;

        if (px >= plot_x && px < plot_x + plot_w && py >= plot_y && py < plot_y + plot_h) {
            set_pixel(pixels, out_width, out_height, px, py, pt_color);
            set_pixel(pixels, out_width, out_height, px+1, py, pt_color);
            set_pixel(pixels, out_width, out_height, px, py+1, pt_color);
            set_pixel(pixels, out_width, out_height, px+1, py+1, pt_color);
        }
    }

    save_image_mem(out_buffer, out_width, out_height, pixels, out_format, jpeg_quality, png_compression);
}
