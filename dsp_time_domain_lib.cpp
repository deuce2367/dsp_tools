#include "dsp_time_domain.hpp"
#include "bluefile_io.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <numeric>
#include <vector>

template<typename T>
void time_domain_data(const std::string& input_file, const std::string& output_file, double start_time, double duration, size_t target_points, double norm_factor) {
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

    spdlog::info("--- Time Domain Envelope Properties ---");
    spdlog::info("Input file:          {}", input_file);
    spdlog::info("Input format:        {} (complex={})", hdr.format, is_complex);
    spdlog::info("Input rate:          {} Hz", sample_rate);
    spdlog::info("Target points:       {}", target_points);
    spdlog::info("Actual points:       {}", actual_points);
    spdlog::info("Aggregation window:  {} frames", frames_per_point);
    
    size_t floats_per_bucket = is_complex ? 4 : 2;
    std::vector<float> time_domain_out(actual_points * floats_per_bucket, 0.0f);

    for (size_t i = 0; i < actual_points; ++i) {
        if (is_complex) {
            float min_i = 1e9f, max_i = -1e9f;
            float min_q = 1e9f, max_q = -1e9f;
            
            for (size_t j = 0; j < frames_per_point; ++j) {
                size_t idx = (i * frames_per_point + j) * 2;
                float i_val = static_cast<float>(in_ptr[idx]) / norm_factor;
                float q_val = static_cast<float>(in_ptr[idx + 1]) / norm_factor;
                
                if (i_val < min_i) min_i = i_val;
                if (i_val > max_i) max_i = i_val;
                if (q_val < min_q) min_q = q_val;
                if (q_val > max_q) max_q = q_val;
            }
            // Interleaved I and Q min/max pairs (so SigPlot draws solid line between I_min->I_max and Q_min->Q_max)
            time_domain_out[i * 4 + 0] = min_i;
            time_domain_out[i * 4 + 1] = min_q;
            time_domain_out[i * 4 + 2] = max_i;
            time_domain_out[i * 4 + 3] = max_q;
        } else {
            float min_v = 1e9f, max_v = -1e9f;
            
            for (size_t j = 0; j < frames_per_point; ++j) {
                size_t idx = (i * frames_per_point + j);
                float val = static_cast<float>(in_ptr[idx]) / norm_factor;
                
                if (val < min_v) min_v = val;
                if (val > max_v) max_v = val;
            }
            time_domain_out[i * 2 + 0] = min_v;
            time_domain_out[i * 2 + 1] = max_v;
        }
    }

    // Update Header for output
    char out_format[2];
    out_format[0] = is_complex ? 'C' : 'S';
    out_format[1] = 'F'; // Float32
    
    hdr.format[0] = out_format[0];
    hdr.format[1] = out_format[1];
    std::strncpy(hdr.head_rep, "EEEI", 4);
    std::strncpy(hdr.data_rep, "EEEI", 4);
    
    // The new xdelta is half the time per point because we output 2 samples per bucket (min and max)
    hdr.xdelta = static_cast<double>(frames_per_point) / sample_rate / 2.0;
    hdr.data_size = time_domain_out.size() * sizeof(float);
    
    // Write out bluefile
    hdr.type = 1000; // Type 1000 is for 1D generic data (used by SigPlot)
    write_bluefile_header(output_file, hdr);
    
    FILE* fout = fopen(output_file.c_str(), "ab");
    if (!fout) throw std::runtime_error("Could not open output file for appending: " + output_file);
    
    fwrite(time_domain_out.data(), sizeof(float), time_domain_out.size(), fout);
    fclose(fout);
    
    spdlog::info("Time domain envelope successfully written to {}", output_file);
}

void DspTimeDomain::generate_time_domain_envelope(
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
        time_domain_data<int8_t>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'I') {
        time_domain_data<int16_t>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'L') {
        time_domain_data<int32_t>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'F') {
        time_domain_data<float>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else if (data_type == 'D') {
        time_domain_data<double>(input_file, output_file, start_time, duration, target_points, norm_factor);
    } else {
        throw std::runtime_error("Unsupported data format type: " + std::string(1, data_type));
    }
}
