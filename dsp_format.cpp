#include "bluefile_io.hpp"
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/io.hpp>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>

using namespace kfr;

// Enum definitions
enum class ComplexMethod { Pad, Hilbert, Pack };
enum class ExtractMethod { I, Q, Mag, Phase, Unpack };

// Function to safely extract a casted type from a character
char get_format_char_from_type(char input_fmt, const std::string& cast_str) {
    if (cast_str.empty()) return input_fmt;
    char c = std::toupper(cast_str[0]);
    if (c == 'B' || c == 'I' || c == 'L' || c == 'F' || c == 'D') return c;
    throw std::runtime_error("Invalid cast format character. Must be B, I, L, F, or D.");
}

template<typename TIn, typename TOut>
void format_data(const std::string& input_file, const std::string& output_file, 
                 bool to_complex, ComplexMethod complex_method, size_t taps,
                 bool to_real, ExtractMethod extract_method) {
    
    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);
    
    bool is_complex = (hdr.format[0] == 'C');
    
    size_t data_offset = static_cast<size_t>(hdr.data_start);
    size_t data_bytes = static_cast<size_t>(hdr.data_size);
    
    if (data_offset + data_bytes > mmap_in.size) {
        throw std::runtime_error("Invalid data_size in bluefile header");
    }
    
    TIn* in_ptr = reinterpret_cast<TIn*>(mmap_in.ptr + data_offset);
    size_t total_samples = data_bytes / sizeof(TIn);
    size_t in_samples_per_frame = is_complex ? 2 : 1;
    size_t num_frames = total_samples / in_samples_per_frame;
    
    double sample_rate = (hdr.xdelta > 0.0) ? (1.0 / hdr.xdelta) : 1.0;
    
    spdlog::info("--- Format Input ---");
    spdlog::info("Input file:          {}", input_file);
    spdlog::info("Input format:        {}", hdr.format);
    spdlog::info("Input frames:        {}", num_frames);
    
    bool out_is_complex = is_complex;
    if (to_complex) out_is_complex = true;
    if (to_real) out_is_complex = false;
    
    if (to_complex && is_complex) {
        spdlog::warn("Input is already complex. Ignoring --to-complex.");
    }
    if (to_real && !is_complex) {
        spdlog::warn("Input is already real. Ignoring --to-real.");
    }
    
    size_t out_samples_per_frame = out_is_complex ? 2 : 1;
    
    // Prepare Output Header
    char out_fmt_char = (sizeof(TOut) == 1) ? 'B' : 
                        (sizeof(TOut) == 2) ? 'I' : 
                        (sizeof(TOut) == 4 && std::is_integral<TOut>::value) ? 'L' : 
                        (sizeof(TOut) == 4 && std::is_floating_point<TOut>::value) ? 'F' : 'D';
    
    hdr.format[0] = out_is_complex ? 'C' : 'S';
    hdr.format[1] = out_fmt_char;
    hdr.format[2] = '\0';
    
    // Handle sample rate adjustments for packing/unpacking
    if (to_complex && !is_complex && complex_method == ComplexMethod::Pack) {
        if (num_frames % 2 != 0) {
            spdlog::warn("Input real frames is not even. The last sample will be dropped.");
            num_frames--;
        }
        num_frames /= 2; // Output has half the number of frames
        hdr.xdelta *= 2.0; // Sample rate is halved
        spdlog::info("Packing enabled: Sample rate halved to {} Hz", 1.0 / hdr.xdelta);
    }
    
    if (to_real && is_complex && extract_method == ExtractMethod::Unpack) {
        num_frames *= 2; // Output has twice the number of frames
        hdr.xdelta /= 2.0; // Sample rate is doubled
        spdlog::info("Unpacking enabled: Sample rate doubled to {} Hz", 1.0 / hdr.xdelta);
    }
    
    hdr.data_size = num_frames * out_samples_per_frame * sizeof(TOut);
    
    double delay_sec = 0.0;
    
    univector<fbase> hilbert_taps;
    std::unique_ptr<filter_fir<fbase>> hilbert_filter;
    
    if (to_complex && !is_complex && complex_method == ComplexMethod::Hilbert) {
        if (taps % 2 == 0) taps++;
        hilbert_taps.resize(taps);
        int center = taps / 2;
        for (int i = 0; i < taps; ++i) {
            int n = i - center;
            if (n == 0 || n % 2 == 0) {
                hilbert_taps[i] = 0.0;
            } else {
                hilbert_taps[i] = 2.0 / (M_PI * n);
            }
        }
        hilbert_taps = hilbert_taps * window_blackman_harris(taps);
        hilbert_filter = std::make_unique<filter_fir<fbase>>(hilbert_taps);
        delay_sec = static_cast<double>(taps / 2) / sample_rate;
        spdlog::info("Hilbert mode enabled. FIR Taps: {}, Delay: {} samples ({} sec)", taps, taps / 2, delay_sec);
    }
    
    spdlog::info("--- Format Output ---");
    spdlog::info("Output format:       {}", hdr.format);
    
    if (hdr.timecode != 0.0 && delay_sec > 0.0) {
        spdlog::info("Original timecode:   {:.9f}", hdr.timecode);
        hdr.timecode -= delay_sec; // group delay correction
        spdlog::info("Corrected timecode:  {:.9f}", hdr.timecode);
    }
    
    std::vector<uint8_t> ext_data = read_bluefile_ext_header(input_file, hdr);
    if (!ext_data.empty()) {
        hdr.ext_start = static_cast<int32_t>(hdr.data_start + hdr.data_size);
    }
    
    write_bluefile_header(output_file, hdr);
    int out_fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
    if (out_fd < 0) throw std::runtime_error("Could not open output file for appending");
    
    const size_t block_size = 1048576; // 1M frames per block
    size_t frames_processed = 0;
    
    std::vector<TOut> out_final(block_size * out_samples_per_frame);
    
    // For Hilbert delay buffer
    std::vector<fbase> delay_buffer;
    if (hilbert_filter) {
        delay_buffer.resize(taps / 2, 0.0);
    }
    
    univector<fbase> in_f(block_size);
    univector<fbase> out_q_f(block_size);
    
    while (frames_processed < num_frames) {
        size_t frames_to_process = std::min(block_size, num_frames - frames_processed);
        
        in_f.resize(frames_to_process);
        out_q_f.resize(frames_to_process);
        
        if (!is_complex && out_is_complex) {
            // Real to Complex
            for (size_t i = 0; i < frames_to_process; ++i) {
                in_f[i] = static_cast<fbase>(in_ptr[frames_processed + i]);
            }
            
            if (complex_method == ComplexMethod::Pad) {
                for (size_t i = 0; i < frames_to_process; ++i) {
                    out_final[i * 2] = static_cast<TOut>(in_f[i]);
                    out_final[i * 2 + 1] = 0;
                }
            } else if (complex_method == ComplexMethod::Pack) {
                for (size_t i = 0; i < frames_to_process; ++i) {
                    size_t m = frames_processed + i;
                    fbase sign = (m % 2 == 0) ? 1.0 : -1.0;
                    fbase i_val = static_cast<fbase>(in_ptr[m * 2]) * sign;
                    fbase q_val = -static_cast<fbase>(in_ptr[m * 2 + 1]) * sign;
                    out_final[i * 2] = static_cast<TOut>(i_val);
                    out_final[i * 2 + 1] = static_cast<TOut>(q_val);
                }
            } else if (complex_method == ComplexMethod::Hilbert) {
                hilbert_filter->apply(out_q_f, in_f);
                
                for (size_t i = 0; i < frames_to_process; ++i) {
                    fbase delayed_i = 0.0;
                    if (i < delay_buffer.size()) {
                        delayed_i = delay_buffer[i];
                    } else {
                        delayed_i = in_f[i - delay_buffer.size()];
                    }
                    
                    out_final[i * 2] = static_cast<TOut>(delayed_i);
                    out_final[i * 2 + 1] = static_cast<TOut>(out_q_f[i]);
                }
                
                // Update delay buffer
                if (frames_to_process >= delay_buffer.size()) {
                    for (size_t i = 0; i < delay_buffer.size(); ++i) {
                        delay_buffer[i] = in_f[frames_to_process - delay_buffer.size() + i];
                    }
                } else {
                    // rare edge case if block_size is smaller than delay buffer
                    std::vector<fbase> new_delay_buffer(delay_buffer.size());
                    size_t shift = frames_to_process;
                    for (size_t i = 0; i < delay_buffer.size() - shift; ++i) {
                        new_delay_buffer[i] = delay_buffer[i + shift];
                    }
                    for (size_t i = 0; i < shift; ++i) {
                        new_delay_buffer[delay_buffer.size() - shift + i] = in_f[i];
                    }
                    delay_buffer = new_delay_buffer;
                }
            }
        } 
        else if (is_complex && !out_is_complex) {
            // Complex to Real
            if (extract_method == ExtractMethod::Unpack) {
                // Unpack mode: Output is double the frames
                for (size_t i = 0; i < frames_to_process / 2; ++i) {
                    size_t m = frames_processed / 2 + i;
                    fbase sign = (m % 2 == 0) ? 1.0 : -1.0;
                    fbase ii = static_cast<fbase>(in_ptr[m * 2]) * sign;
                    fbase qq = -static_cast<fbase>(in_ptr[m * 2 + 1]) * sign;
                    out_final[i * 2] = static_cast<TOut>(ii);
                    out_final[i * 2 + 1] = static_cast<TOut>(qq);
                }
            } else {
                for (size_t i = 0; i < frames_to_process; ++i) {
                    fbase ii = static_cast<fbase>(in_ptr[(frames_processed + i) * 2]);
                    fbase qq = static_cast<fbase>(in_ptr[(frames_processed + i) * 2 + 1]);
                    
                    if (extract_method == ExtractMethod::I) {
                        out_final[i] = static_cast<TOut>(ii);
                    } else if (extract_method == ExtractMethod::Q) {
                        out_final[i] = static_cast<TOut>(qq);
                    } else if (extract_method == ExtractMethod::Mag) {
                        out_final[i] = static_cast<TOut>(std::sqrt(ii*ii + qq*qq));
                    } else if (extract_method == ExtractMethod::Phase) {
                        out_final[i] = static_cast<TOut>(std::atan2(qq, ii));
                    }
                }
            }
        }
        else {
            // Just Type Casting (Real to Real or Complex to Complex)
            for (size_t i = 0; i < frames_to_process * out_samples_per_frame; ++i) {
                out_final[i] = static_cast<TOut>(in_ptr[frames_processed * in_samples_per_frame + i]);
            }
        }
        
        ssize_t bytes_to_write = frames_to_process * out_samples_per_frame * sizeof(TOut);
        const uint8_t* out_bytes = reinterpret_cast<const uint8_t*>(out_final.data());
        size_t written = 0;
        while (written < bytes_to_write) {
            ssize_t res = write(out_fd, out_bytes + written, bytes_to_write - written);
            if (res < 0) {
                close(out_fd);
                throw std::runtime_error("Failed writing data");
            }
            written += res;
        }
        
        frames_processed += frames_to_process;
    }
    
    close(out_fd);
    write_bluefile_ext_header(output_file, ext_data);
    spdlog::info("Formatting complete.");
}

// Template dispatcher to avoid massive switch statements for both TIn and TOut
template<typename TIn>
void dispatch_out(char out_type, const std::string& input_file, const std::string& output_file, 
                  bool to_complex, ComplexMethod complex_method, size_t taps,
                  bool to_real, ExtractMethod extract_method) {
    if (out_type == 'B') format_data<TIn, int8_t>(input_file, output_file, to_complex, complex_method, taps, to_real, extract_method);
    else if (out_type == 'I') format_data<TIn, int16_t>(input_file, output_file, to_complex, complex_method, taps, to_real, extract_method);
    else if (out_type == 'L') format_data<TIn, int32_t>(input_file, output_file, to_complex, complex_method, taps, to_real, extract_method);
    else if (out_type == 'F') format_data<TIn, float>(input_file, output_file, to_complex, complex_method, taps, to_real, extract_method);
    else if (out_type == 'D') format_data<TIn, double>(input_file, output_file, to_complex, complex_method, taps, to_real, extract_method);
    else throw std::runtime_error("Unsupported output data type");
}

int main(int argc, char** argv) {
    CLI::App app{"DSP Format for X-Midas Bluefiles\n"
                 "Converts data formats between Real (Scalar) and Complex, and changes data types (e.g. float to double)."};
    
    std::string input_file;
    std::string output_file;
    bool to_complex = false;
    bool to_real = false;
    std::string method_str = "pad";
    std::string extract_str = "i";
    std::string cast_str = "";
    size_t taps = 127;
    
    app.add_option("-i,--input", input_file, "Input Bluefile (.prm)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    
    app.add_flag("--to-complex", to_complex, "Convert Real input to Complex output");
    app.add_flag("--to-real", to_real, "Convert Complex input to Real output");
    
    app.add_option("--method", method_str, "Method for Real -> Complex (pad, hilbert, pack). Default: pad");
    app.add_option("--extract", extract_str, "Extraction for Complex -> Real (i, q, mag, phase, unpack). Default: i");
    app.add_option("--cast", cast_str, "Target data type character (B, I, L, F, D). Default: Keep same as input.");
    app.add_option("--taps", taps, "Number of FIR taps for Hilbert transform (must be odd, default: 127)");
    
    CLI11_PARSE(app, argc, argv);
    
    if (to_complex && to_real) {
        spdlog::error("Cannot specify both --to-complex and --to-real");
        return 1;
    }
    
    ComplexMethod cmethod = ComplexMethod::Pad;
    if (method_str == "pad") cmethod = ComplexMethod::Pad;
    else if (method_str == "hilbert") cmethod = ComplexMethod::Hilbert;
    else if (method_str == "pack") cmethod = ComplexMethod::Pack;
    else { spdlog::error("Invalid --method"); return 1; }
    
    ExtractMethod emethod = ExtractMethod::I;
    if (extract_str == "i") emethod = ExtractMethod::I;
    else if (extract_str == "q") emethod = ExtractMethod::Q;
    else if (extract_str == "mag") emethod = ExtractMethod::Mag;
    else if (extract_str == "phase") emethod = ExtractMethod::Phase;
    else if (extract_str == "unpack") emethod = ExtractMethod::Unpack;
    else { spdlog::error("Invalid --extract"); return 1; }
    
    try {
        BlueHeader hdr = read_bluefile_header(input_file);
        char in_type = hdr.format[1];
        char out_type = get_format_char_from_type(in_type, cast_str);
        
        if (in_type == 'B') dispatch_out<int8_t>(out_type, input_file, output_file, to_complex, cmethod, taps, to_real, emethod);
        else if (in_type == 'I') dispatch_out<int16_t>(out_type, input_file, output_file, to_complex, cmethod, taps, to_real, emethod);
        else if (in_type == 'L') dispatch_out<int32_t>(out_type, input_file, output_file, to_complex, cmethod, taps, to_real, emethod);
        else if (in_type == 'F') dispatch_out<float>(out_type, input_file, output_file, to_complex, cmethod, taps, to_real, emethod);
        else if (in_type == 'D') dispatch_out<double>(out_type, input_file, output_file, to_complex, cmethod, taps, to_real, emethod);
        else throw std::runtime_error("Unsupported input data type format");
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
