#include "bluefile_io.hpp"
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>
#include <kfr/io.hpp>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <vector>
#include <deque>

using namespace kfr;

template <typename T>
void process_whitener(const std::string& input_file, const std::string& output_file,
                      size_t fft_size, double alpha, double blank_threshold, size_t blank_window, double output_gain, double strength) {
    
    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);
    
    bool is_complex = (hdr.format[0] == 'C');
    size_t in_samples_per_frame = is_complex ? 2 : 1;
    
    size_t data_offset = static_cast<size_t>(hdr.data_start);
    size_t data_bytes = static_cast<size_t>(hdr.data_size);
    if (data_offset + data_bytes > mmap_in.size) {
        throw std::runtime_error("Invalid data_size in bluefile header");
    }
    
    T* in_ptr = reinterpret_cast<T*>(mmap_in.ptr + data_offset);
    size_t num_frames = data_bytes / (in_samples_per_frame * sizeof(T));
    
    spdlog::info("--- Whitener Setup ---");
    spdlog::info("Input frames:        {}", num_frames);
    spdlog::info("Format:              {}", hdr.format);
    spdlog::info("FFT Size:            {}", fft_size);
    spdlog::info("Alpha (Smoothing):   {}", alpha);
    spdlog::info("Blank Threshold:     {}x Average Power", blank_threshold);
    
    // Setup output file
    std::vector<uint8_t> ext_data = read_bluefile_ext_header(input_file, hdr);
    write_bluefile_header(output_file, hdr);
    int out_fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
    if (out_fd < 0) throw std::runtime_error("Could not open output file");
    
    // --- STFT Engine Setup ---
    size_t step_size = fft_size / 2; // 50% overlap
    
    // Hann window for 50% overlap perfect reconstruction
    std::vector<fbase> window(fft_size);
    for (size_t i = 0; i < fft_size; ++i) {
        window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / fft_size));
    }
    
    std::vector<fbase> avg_mag(fft_size, 0.0);
    bool mag_initialized = false;
    
    const fbase EPSILON = 1e-12;
    
    // Plans
    dft_plan_real<fbase>* plan_r = nullptr;
    dft_plan<fbase>* plan_c = nullptr;
    std::vector<u8> temp_r, temp_c;
    
    if (is_complex) {
        plan_c = new dft_plan<fbase>(fft_size);
        temp_c.resize(plan_c->temp_size);
    } else {
        plan_r = new dft_plan_real<fbase>(fft_size);
        temp_r.resize(plan_r->temp_size);
    }
    
    // Buffers
    std::vector<complex<fbase>> c_in(fft_size), c_out(fft_size);
    std::vector<fbase> r_in(fft_size), r_out(fft_size);
    
    // OLA Output Buffer (holds current assembled output)
    std::vector<T> ola_buffer(fft_size * in_samples_per_frame, 0);
    
    // Read buffer
    std::vector<T> read_buffer(fft_size * in_samples_per_frame, 0);
    
    size_t frames_read = 0;
    
    // Blanking state
    fbase run_power = 0.0;
    fbase blank_alpha = 1.0 / blank_window;
    size_t blank_count = 0;
    
    while (frames_read < num_frames) {
        // Shift OLA buffer down by step_size
        size_t keep_frames = fft_size - step_size;
        
        // Write the finished part of OLA buffer to file
        // The first `step_size` frames are fully overlapped and finished!
        if (frames_read >= fft_size) { // don't write before first full block is processed
            ssize_t bytes_to_write = step_size * in_samples_per_frame * sizeof(T);
            const uint8_t* out_bytes = reinterpret_cast<const uint8_t*>(ola_buffer.data());
            size_t written = 0;
            while (written < bytes_to_write) {
                ssize_t res = write(out_fd, out_bytes + written, bytes_to_write - written);
                if (res < 0) throw std::runtime_error("Write failed");
                written += res;
            }
        }
        
        // Shift OLA buffer
        for (size_t i = 0; i < keep_frames * in_samples_per_frame; ++i) {
            ola_buffer[i] = ola_buffer[step_size * in_samples_per_frame + i];
        }
        for (size_t i = keep_frames * in_samples_per_frame; i < ola_buffer.size(); ++i) {
            ola_buffer[i] = 0;
        }
        
        // Read next block of data. We need to fill `fft_size` frames.
        size_t new_frames_to_read;
        size_t read_offset;
        
        if (frames_read == 0) {
            // First iteration: read a full fft_size block to avoid zero-padding transient!
            new_frames_to_read = std::min(fft_size, num_frames);
            read_offset = 0;
        } else {
            // Normal iteration: shift read_buffer and read step_size new frames
            for (size_t i = 0; i < keep_frames * in_samples_per_frame; ++i) {
                read_buffer[i] = read_buffer[step_size * in_samples_per_frame + i];
            }
            new_frames_to_read = std::min(step_size, num_frames - frames_read);
            read_offset = keep_frames * in_samples_per_frame;
        }
        
        for (size_t i = 0; i < new_frames_to_read * in_samples_per_frame; ++i) {
            T val = in_ptr[frames_read * in_samples_per_frame + i];
            
            // --- Pulse Blanking Logic ---
            fbase p = static_cast<fbase>(val) * static_cast<fbase>(val);
            if (is_complex && i % 2 == 1) {
                fbase i_val = static_cast<fbase>(in_ptr[frames_read * in_samples_per_frame + i - 1]);
                p += i_val * i_val;
            } else if (is_complex) {
                p = 0.0; // wait for Q to process
            }
            
            if (!is_complex || i % 2 == 1) {
                // Initialize power tracker quickly
                if (frames_read == 0 && i < in_samples_per_frame * 10) {
                    run_power = p; 
                } 
                
                // If power spikes drastically over average, blank it
                if (p > blank_threshold * run_power && run_power > 0.0) {
                    val = 0;
                    if (is_complex) {
                        read_buffer[read_offset + i - 1] = 0; // zero out I as well
                    }
                    blank_count++;
                } else {
                    // Only update run_power if it's NOT a massive pulse
                    run_power = (1.0 - blank_alpha) * run_power + blank_alpha * p;
                }
            }
            
            read_buffer[read_offset + i] = val;
        }
        
        // Zero pad if we hit EOF
        size_t expected_new_frames = (frames_read == 0) ? fft_size : step_size;
        for (size_t i = new_frames_to_read * in_samples_per_frame; i < expected_new_frames * in_samples_per_frame; ++i) {
            read_buffer[read_offset + i] = 0;
        }
        
        // --- STFT Processing ---
        if (is_complex) {
            for (size_t i = 0; i < fft_size; ++i) {
                fbase iv = static_cast<fbase>(read_buffer[i * 2]);
                fbase qv = static_cast<fbase>(read_buffer[i * 2 + 1]);
                c_in[i] = complex<fbase>(iv, qv) * window[i];
            }
            plan_c->execute(c_out.data(), c_in.data(), temp_c.data());
            
            // Whiten
            for (size_t i = 0; i < fft_size; ++i) {
                fbase mag = kfr::cabs(c_out[i]);
                if (!mag_initialized) avg_mag[i] = mag;
                else avg_mag[i] = alpha * avg_mag[i] + (1.0 - alpha) * mag;
                
                fbase divisor = std::max(avg_mag[i], EPSILON);
                if (strength != 1.0) divisor = std::pow(divisor, strength);
                c_out[i] = c_out[i] / divisor;
            }
            mag_initialized = true;
            
            plan_c->execute(c_in.data(), c_out.data(), temp_c.data(), kfr::cinvert_t()); // Inverse FFT
            
            // Calculate power of the windowed IFFT output
            fbase p_win_new = 0.0;
            for (size_t i = 0; i < fft_size; ++i) {
                fbase iv = c_in[i].real() / fft_size;
                fbase qv = c_in[i].imag() / fft_size;
                p_win_new += (iv * iv + qv * qv);
            }
            p_win_new /= fft_size;
            
            // Scale to restore original noise floor. Hann window power factor is 3/8 = 0.375
            fbase target_power = run_power * 0.375;
            fbase scale = (output_gain > 0.0) ? output_gain : std::sqrt(std::max(target_power, EPSILON) / std::max(p_win_new, EPSILON));
            
            // OLA
            for (size_t i = 0; i < fft_size; ++i) {
                // IFFT needs scaling by 1/N
                fbase iv = c_in[i].real() / fft_size;
                fbase qv = c_in[i].imag() / fft_size;
                ola_buffer[i * 2] += static_cast<T>(iv * scale);
                ola_buffer[i * 2 + 1] += static_cast<T>(qv * scale);
            }
            
        } else {
            for (size_t i = 0; i < fft_size; ++i) {
                r_in[i] = static_cast<fbase>(read_buffer[i]) * window[i];
            }
            plan_r->execute(c_out.data(), r_in.data(), temp_r.data());
            
            // Whiten (only positive frequencies up to Nyquist for Real FFT)
            size_t bins = fft_size / 2 + 1;
            for (size_t i = 0; i < bins; ++i) {
                fbase mag = kfr::cabs(c_out[i]);
                if (!mag_initialized) avg_mag[i] = mag;
                else avg_mag[i] = alpha * avg_mag[i] + (1.0 - alpha) * mag;
                
                fbase divisor = std::max(avg_mag[i], EPSILON);
                if (strength != 1.0) divisor = std::pow(divisor, strength);
                c_out[i] = c_out[i] / divisor;
            }
            mag_initialized = true;
            
            plan_r->execute(r_out.data(), c_out.data(), temp_r.data(), kfr::cinvert_t()); // Inverse FFT
            
            // Calculate power of the windowed IFFT output
            fbase p_win_new = 0.0;
            for (size_t i = 0; i < fft_size; ++i) {
                fbase v = r_out[i] / fft_size;
                p_win_new += (v * v);
            }
            p_win_new /= fft_size;
            
            // Scale to restore original noise floor. Hann window power factor is 0.375
            fbase target_power = run_power * 0.375;
            fbase scale = (output_gain > 0.0) ? output_gain : std::sqrt(std::max(target_power, EPSILON) / std::max(p_win_new, EPSILON));
            
            // OLA
            for (size_t i = 0; i < fft_size; ++i) {
                fbase v = r_out[i] / fft_size;
                ola_buffer[i] += static_cast<T>(v * scale);
            }
        }
        
        frames_read += new_frames_to_read;
    }
    
    // Flush the last part of the OLA buffer
    ssize_t bytes_to_write = fft_size * in_samples_per_frame * sizeof(T);
    const uint8_t* out_bytes = reinterpret_cast<const uint8_t*>(ola_buffer.data());
    size_t written = 0;
    while (written < bytes_to_write) {
        ssize_t res = write(out_fd, out_bytes + written, bytes_to_write - written);
        if (res < 0) throw std::runtime_error("Write failed");
        written += res;
    }
    
    close(out_fd);
    write_bluefile_ext_header(output_file, ext_data);
    
    spdlog::info("Whitening complete. Blanked {} pulse samples.", blank_count);
    
    if (plan_r) delete plan_r;
    if (plan_c) delete plan_c;
}

int main(int argc, char** argv) {
    CLI::App app{"DSP Whitener for X-Midas Bluefiles\n"
                 "Flattens the frequency spectrum and dynamically blanks radar/pulse interference."};
    
    std::string input_file;
    std::string output_file;
    size_t fft_size = 4096;
    double alpha = 0.99;
    double blank_threshold = 10.0;
    size_t blank_window = 1024;
    double output_gain = 0.0;
    double strength = 0.5; // Default to partial whitening
    
    app.add_option("-i,--input", input_file, "Input Bluefile (.prm)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    
    app.add_option("--fft-size", fft_size, "FFT size for STFT (default: 4096)");
    app.add_option("--alpha", alpha, "Exponential smoothing factor for spectrum [0.0 to 1.0) (default: 0.99)");
    app.add_option("--blank-threshold", blank_threshold, "Power multiplier threshold to trigger blanking. Set to 0 to disable blanking. (default: 10.0)");
    app.add_option("--blank-window", blank_window, "Rolling average window size for blanker (default: 1024)");
    app.add_option("--gain", output_gain, "Fixed output gain multiplier. Set to 0.0 for auto-restoration of original noise floor. (default: 0.0)");
    app.add_option("--strength", strength, "Whitening strength from 0.0 (none) to 1.0 (full flattening). (default: 0.5)");
    
    CLI11_PARSE(app, argc, argv);
    
    try {
        BlueHeader hdr = read_bluefile_header(input_file);
        char in_type = hdr.format[1];
        
        if (in_type == 'B') process_whitener<int8_t>(input_file, output_file, fft_size, alpha, blank_threshold, blank_window, output_gain, strength);
        else if (in_type == 'I') process_whitener<int16_t>(input_file, output_file, fft_size, alpha, blank_threshold, blank_window, output_gain, strength);
        else if (in_type == 'L') process_whitener<int32_t>(input_file, output_file, fft_size, alpha, blank_threshold, blank_window, output_gain, strength);
        else if (in_type == 'F') process_whitener<float>(input_file, output_file, fft_size, alpha, blank_threshold, blank_window, output_gain, strength);
        else if (in_type == 'D') process_whitener<double>(input_file, output_file, fft_size, alpha, blank_threshold, blank_window, output_gain, strength);
        else throw std::runtime_error("Unsupported input data type format");
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
