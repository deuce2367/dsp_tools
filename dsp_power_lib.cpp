#include "dsp_power.hpp"
#include "bluefile_io.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <numeric>
#include <vector>

template<typename T>
void power_data(const std::string& input_file, const std::string& output_file, double start_time, double duration, size_t target_points, double norm_factor) {
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
    
    // Time slicing
    size_t frames_to_skip = static_cast<size_t>(start_time * sample_rate);
    size_t frames_to_process = (duration > 0.0) ? static_cast<size_t>(duration * sample_rate) : num_frames - frames_to_skip;
    
    if (frames_to_skip >= num_frames) {
        throw std::runtime_error("Start time is beyond the end of the file");
    }
    if (frames_to_skip + frames_to_process > num_frames) {
        frames_to_process = num_frames - frames_to_skip;
    }
    
    in_ptr += frames_to_skip * samples_per_frame;
    num_frames = frames_to_process;
    
    if (target_points == 0) target_points = 10000;
    if (target_points > num_frames) target_points = num_frames;

    size_t frames_per_point = num_frames / target_points;
    if (frames_per_point == 0) frames_per_point = 1;

    size_t actual_points = num_frames / frames_per_point;

    spdlog::info("--- Power Envelope Properties ---");
    spdlog::info("Input file:          {}", input_file);
    spdlog::info("Input format:        {} (complex={})", hdr.format, is_complex);
    spdlog::info("Input rate:          {} Hz", sample_rate);
    spdlog::info("Target points:       {}", target_points);
    spdlog::info("Actual points:       {}", actual_points);
    spdlog::info("Aggregation window:  {} frames", frames_per_point);
    
    std::vector<float> power_db(actual_points, -200.0f);

    for (size_t i = 0; i < actual_points; ++i) {
        double max_power = 0.0;
        for (size_t j = 0; j < frames_per_point; ++j) {
            size_t idx = (i * frames_per_point + j) * samples_per_frame;
            double p = 0.0;
            if (is_complex) {
                double i_val = static_cast<double>(in_ptr[idx]) / norm_factor;
                double q_val = static_cast<double>(in_ptr[idx + 1]) / norm_factor;
                p = i_val * i_val + q_val * q_val;
            } else {
                double val = static_cast<double>(in_ptr[idx]) / norm_factor;
                p = val * val;
            }
            if (p > max_power) {
                max_power = p;
            }
        }
        
        // Convert to dB
        if (max_power > 1e-20) {
            power_db[i] = static_cast<float>(10.0 * std::log10(max_power));
        } else {
            power_db[i] = -200.0f; // Noise floor limit
        }
    }

    // Output is ALWAYS real floats (format SF)
    char out_format[2];
    out_format[0] = 'S';
    out_format[1] = 'F'; // Float32
    
    hdr.format[0] = out_format[0];
    hdr.format[1] = out_format[1];
    
    // The new xdelta is the time per point
    hdr.xdelta = static_cast<double>(frames_per_point) / sample_rate;
    hdr.data_size = actual_points * sizeof(float);
    
    // Write out bluefile
    hdr.type = 1000; // Type 1000 is for 1D generic data (used by SigPlot)
    write_bluefile_header(output_file, hdr);
    
    FILE* fout = fopen(output_file.c_str(), "ab");
    if (!fout) throw std::runtime_error("Could not open output file for appending: " + output_file);
    
    fwrite(power_db.data(), sizeof(float), power_db.size(), fout);
    fclose(fout);
    
    spdlog::info("Power envelope successfully written to {}", output_file);
}

void DspPower::generate_power_envelope(
    const std::string& input_file,
    const std::string& output_file,
    double start_time,
    double duration,
    size_t target_points
) {
    BlueHeader hdr = read_bluefile_header(input_file);
    
    char data_type = hdr.format[1];
    double norm_factor = 1.0;
    if (data_type == 'B') norm_factor = 128.0;
    else if (data_type == 'I') norm_factor = 32768.0;
    else if (data_type == 'L') norm_factor = 2147483648.0;

    if (data_type == 'B') {
        power_data<int8_t>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'I') {
        power_data<int16_t>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'L') {
        power_data<int32_t>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'F') {
        power_data<float>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'D') {
        power_data<double>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else {
        throw std::runtime_error("Unsupported data format type: " + std::string(1, data_type));
    }
}
