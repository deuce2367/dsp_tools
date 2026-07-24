#include "bluefile_io.hpp"
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>
#include <kfr/dft/convolution.hpp>

#include <spdlog/spdlog.h>
#include <string>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <cmath>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace kfr;

template<typename T>
void filter_data(const std::string& input_file, const std::string& output_file, const std::string& filter_type, double cutoff1, double cutoff2, size_t taps, double center_freq = 0.0) {
    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);
    
    double sample_rate = (hdr.xdelta > 0.0) ? (1.0 / hdr.xdelta) : 1.0;
    bool is_complex = (hdr.format[0] == 'C');
    
    size_t data_offset = static_cast<size_t>(hdr.data_start);
    size_t data_bytes = static_cast<size_t>(hdr.data_size);
    
    if (data_offset + data_bytes > mmap_in.size) {
        throw std::runtime_error("Invalid data_size in bluefile header");
    }
    
    T* in_ptr = reinterpret_cast<T*>(mmap_in.ptr + data_offset);
    size_t total_samples = data_bytes / sizeof(T);
    size_t samples_per_frame = is_complex ? 2 : 1;
    size_t num_frames = total_samples / samples_per_frame;
    
    
    auto start_time = std::chrono::high_resolution_clock::now();

    spdlog::info("Input rate: {} Hz, Frames: {}", sample_rate, num_frames);
    spdlog::info("Format: {} (complex={})", hdr.format, is_complex);
    spdlog::info("Filter: {}, cutoff1: {} Hz, cutoff2: {} Hz, taps: {}, center_freq: {} Hz", filter_type, cutoff1, cutoff2, taps, center_freq);
    
    double f_shift = 0.0;
    fbase f_cutoff1 = 0.0;
    fbase f_cutoff2 = 0.0;
    
    if (is_complex && center_freq != 0.0) {
        // Complex signals require frequency translation to apply asymmetric real FIR filters.
        double w_left = center_freq - sample_rate / 2.0;
        double w_right = center_freq + sample_rate / 2.0;
        
        if (filter_type == "lowpass") {
            double pass_width = cutoff1 - w_left;
            double f_c = (w_left + cutoff1) / 2.0;
            f_shift = f_c - center_freq;
            f_cutoff1 = (pass_width / 2.0) / sample_rate;
        } else if (filter_type == "highpass") {
            double pass_width = w_right - cutoff1;
            double f_c = (cutoff1 + w_right) / 2.0;
            f_shift = f_c - center_freq;
            f_cutoff1 = (pass_width / 2.0) / sample_rate;
            // Actually, we use a real lowpass and shift it!
        } else if (filter_type == "bandpass") {
            double pass_width = cutoff2 - cutoff1;
            double f_c = (cutoff1 + cutoff2) / 2.0;
            f_shift = f_c - center_freq;
            f_cutoff1 = (pass_width / 2.0) / sample_rate;
            // Again, a real lowpass is sufficient when shifted
        } else if (filter_type == "bandstop") {
            double stop_width = cutoff2 - cutoff1;
            double f_c = (cutoff1 + cutoff2) / 2.0;
            f_shift = f_c - center_freq;
            f_cutoff1 = (stop_width / 2.0) / sample_rate;
            // We use a real highpass and shift it to stop this specific band
        }
    } else {
        f_cutoff1 = cutoff1 / sample_rate;
        f_cutoff2 = cutoff2 / sample_rate;
    }

    univector<fbase> filter_taps(taps);
    
    if (is_complex && center_freq != 0.0) {
        // We transformed all filters into shifted lowpass (or highpass for bandstop)
        if (filter_type == "bandstop") {
            fir_highpass(filter_taps, f_cutoff1, to_handle(window_blackman_harris(taps)), true);
        } else {
            fir_lowpass(filter_taps, f_cutoff1, to_handle(window_blackman_harris(taps)), true);
        }
    } else {
        if (filter_type == "lowpass") {
            fir_lowpass(filter_taps, f_cutoff1, to_handle(window_blackman_harris(taps)), true);
        } else if (filter_type == "highpass") {
            fir_highpass(filter_taps, f_cutoff1, to_handle(window_blackman_harris(taps)), true);
        } else if (filter_type == "bandpass") {
            fir_bandpass(filter_taps, f_cutoff1, f_cutoff2, to_handle(window_blackman_harris(taps)), true);
        } else if (filter_type == "bandstop") {
            fir_bandstop(filter_taps, f_cutoff1, f_cutoff2, to_handle(window_blackman_harris(taps)), true);
        } else {
            throw std::runtime_error("Unsupported filter type");
        }
    }
    
    univector<fbase> taps_copy = filter_taps;
    convolve_filter<fbase> filter_i(filter_taps, 4096);
    convolve_filter<fbase> filter_q(taps_copy, 4096); // only used if complex
    
    spdlog::info("--- Filter Properties ---");
    spdlog::info("Filter type:         {}", filter_type);
    spdlog::info("Data format:         {} (complex={})", hdr.format, is_complex);
    spdlog::info("Sample rate:         {} Hz", sample_rate);
    spdlog::info("Cutoff 1:            {} Hz", cutoff1);
    if (cutoff2 > 0.0) spdlog::info("Cutoff 2:            {} Hz", cutoff2);
    spdlog::info("Filter taps:         {}", taps);
    spdlog::info("Window type:         Blackman-Harris");
    spdlog::info("Input frames:        {}", num_frames);
    spdlog::info("-------------------------");
    
    double delay_sec = static_cast<double>(taps / 2) / sample_rate;
    spdlog::info("--- Offset Correction ---");
    spdlog::info("Filter delay:        {} input samples", taps / 2);
    spdlog::info("Timecode shift:      {} seconds", delay_sec);
    
    if (hdr.timecode != 0.0) {
        spdlog::info("Original timecode:   {:.9f}", hdr.timecode);
        hdr.timecode -= delay_sec;
        spdlog::info("Corrected timecode:  {:.9f}", hdr.timecode);
    } else {
        spdlog::info("Original timecode is 0.0, skipping correction.");
    }
    spdlog::info("-------------------------");
    
    std::vector<uint8_t> ext_data = read_bluefile_ext_header(input_file, hdr);
    if (!ext_data.empty()) {
        hdr.ext_start = static_cast<int32_t>(hdr.data_start + hdr.data_size); // data_size remains the same for filter
    }
    
    spdlog::info("Filter group delay: {} samples ({} sec)", taps / 2, delay_sec);
    
    write_bluefile_header(output_file, hdr);
    int out_fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
    if (out_fd < 0) throw std::runtime_error("Could not open output file for appending");
    
    // Block process to save memory
    const size_t block_size = 1048576; // 1M frames per block
    size_t frames_processed = 0;
    
    univector<fbase> in_i(block_size), in_q(block_size);
    univector<fbase> out_i(block_size), out_q(block_size);
    std::vector<T> out_final(block_size * samples_per_frame);
    
    
    double phase_shift = 2.0 * M_PI * f_shift / sample_rate;
    double current_phase = 0.0;
    
    while (frames_processed < num_frames) {
        size_t frames_to_process = std::min(block_size, num_frames - frames_processed);
        
        in_i.resize(frames_to_process);
        out_i.resize(frames_to_process);
        if (is_complex) {
            in_q.resize(frames_to_process);
            out_q.resize(frames_to_process);
            
            std::vector<double> phase_cos(frames_to_process);
            std::vector<double> phase_sin(frames_to_process);
            
            double step_cos = std::cos(phase_shift);
            double step_sin = std::sin(phase_shift);
            double current_cos = std::cos(current_phase);
            double current_sin = std::sin(current_phase);
            
            for (size_t i = 0; i < frames_to_process; ++i) {
                double c = current_cos;
                double s = current_sin;
                phase_cos[i] = c;
                phase_sin[i] = s;
                
                double real_in = static_cast<fbase>(in_ptr[(frames_processed + i) * 2]);
                double imag_in = static_cast<fbase>(in_ptr[(frames_processed + i) * 2 + 1]);
                
                // Shift down
                in_i[i] = real_in * c + imag_in * s;
                in_q[i] = imag_in * c - real_in * s;
                
                // Advance phase
                double next_c = current_cos * step_cos - current_sin * step_sin;
                double next_s = current_sin * step_cos + current_cos * step_sin;
                current_cos = next_c;
                current_sin = next_s;
            }
            
            // Re-normalize phase to prevent drift
            current_phase += frames_to_process * phase_shift;
            current_phase = fmod(current_phase, 2.0 * M_PI);
            
            #pragma omp parallel sections
            {
                #pragma omp section
                {
                    filter_i.apply(out_i, in_i);
                }
                #pragma omp section
                {
                    filter_q.apply(out_q, in_q);
                }
            }
            
            for (size_t i = 0; i < frames_to_process; ++i) {
                double c = phase_cos[i];
                double s = phase_sin[i];
                
                double real_out = out_i[i];
                double imag_out = out_q[i];
                
                // Shift up
                double final_real = real_out * c - imag_out * s;
                double final_imag = imag_out * c + real_out * s;
                
                out_final[i * 2] = static_cast<T>(final_real);
                out_final[i * 2 + 1] = static_cast<T>(final_imag);
            }
        } else {
            for (size_t i = 0; i < frames_to_process; ++i) {
                in_i[i] = static_cast<fbase>(in_ptr[frames_processed + i]);
            }
            filter_i.apply(out_i, in_i);
            for (size_t i = 0; i < frames_to_process; ++i) {
                out_final[i] = static_cast<T>(out_i[i]);
            }
        }

        
        ssize_t bytes_to_write = frames_to_process * samples_per_frame * sizeof(T);
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
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;
    spdlog::info("Filtering complete. Runtime: {:.3f} seconds", diff.count());
}

void run_filter_pipeline(const std::string& input_file, const std::string& output_file, const std::string& filter_type, double cutoff1, double cutoff2, size_t taps, double center_freq) {
    BlueHeader hdr = read_bluefile_header(input_file);
    char in_type = hdr.format[1];
    
    if (in_type == 'B') filter_data<int8_t>(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq);
    else if (in_type == 'I') filter_data<int16_t>(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq);
    else if (in_type == 'L') filter_data<int32_t>(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq);
    else if (in_type == 'F') filter_data<float>(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq);
    else if (in_type == 'D') filter_data<double>(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq);
    else throw std::runtime_error("Unsupported input data type format");
}

#ifndef DSP_TOOLS_PYTHON_MODULE
#ifndef DSP_TOOLS_TEST_MODE
#include <CLI/CLI.hpp>
int main(int argc, char** argv) {
    CLI::App app{"DSP Filter for X-Midas Bluefiles\n"
                 "High-performance FIR filtering utility supporting lowpass, highpass, bandpass, and bandstop filters.\n"
                 "Supports real and complex inputs and automatically updates the timecode (in seconds) to compensate for filter group delay."};
    
    std::string input_file;
    std::string output_file;
    std::string filter_type = "lowpass";
    double cutoff1 = 0.0;
    double cutoff2 = 0.0;
    double center_freq = 0.0;
    size_t taps = 1023; // default taps
    
    app.add_option("-i,--input", input_file, "Input Bluefile (.prm)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    app.add_option("-t,--type", filter_type, "Filter Type (lowpass, highpass, bandpass, bandstop)")->required();
    app.add_option("--cutoff1", cutoff1, "Cutoff Frequency 1 (Hz)")->required();
    app.add_option("--cutoff2", cutoff2, "Cutoff Frequency 2 (Hz) (required for bandpass and bandstop filters)");
    app.add_option("--center_freq", center_freq, "Center Frequency of the file (Hz)");
    app.add_option("--taps", taps, "Number of FIR taps (must be odd for Type I linear phase filters). Default is 1023.");
    
    CLI11_PARSE(app, argc, argv);
    
    if (taps % 2 == 0) taps++; // force odd for symmetry
    
    try {
        run_filter_pipeline(input_file, output_file, filter_type, cutoff1, cutoff2, taps, center_freq);
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
#endif
#endif
