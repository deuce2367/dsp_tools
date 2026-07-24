#include "dsp_time_domain.hpp"
#include "bluefile_io.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <numeric>
#include <vector>

template<typename T>
std::vector<uint8_t> time_domain_data(const std::string& input_file, double start_time, double duration, size_t target_points, double norm_factor, const std::string& mode) {
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
    
    bool output_complex = is_complex && (mode == "complex" || mode.empty());
    size_t floats_per_bucket = output_complex ? 4 : 2;
    std::vector<float> time_domain_out(actual_points * floats_per_bucket, 0.0f);

    for (size_t i = 0; i < actual_points; ++i) {
        if (output_complex) {
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
            time_domain_out[i * 4 + 0] = min_i;
            time_domain_out[i * 4 + 1] = min_q;
            time_domain_out[i * 4 + 2] = max_i;
            time_domain_out[i * 4 + 3] = max_q;
        } else {
            float min_v = 1e9f, max_v = -1e9f;
            
            for (size_t j = 0; j < frames_per_point; ++j) {
                float val = 0.0f;
                if (is_complex) {
                    size_t idx = (i * frames_per_point + j) * 2;
                    float i_val = static_cast<float>(in_ptr[idx]) / norm_factor;
                    float q_val = static_cast<float>(in_ptr[idx + 1]) / norm_factor;
                    if (mode == "imag") val = q_val;
                    else if (mode == "magnitude") val = std::sqrt(i_val * i_val + q_val * q_val);
                    else if (mode == "phase") val = std::atan2(q_val, i_val);
                    else val = i_val; // default real
                } else {
                    size_t idx = (i * frames_per_point + j);
                    val = static_cast<float>(in_ptr[idx]) / norm_factor;
                }
                
                if (val < min_v) min_v = val;
                if (val > max_v) max_v = val;
            }
            time_domain_out[i * 2 + 0] = min_v;
            time_domain_out[i * 2 + 1] = max_v;
        }
    }

    // Update Header for output
    char out_format[2];
    out_format[0] = output_complex ? 'C' : 'S';
    out_format[1] = 'F'; // Float32
    
    hdr.format[0] = out_format[0];
    hdr.format[1] = out_format[1];
    std::strncpy(hdr.head_rep, "EEEI", 4);
    std::strncpy(hdr.data_rep, "EEEI", 4);
    
    // The new xdelta is half the time per point because we output 2 samples per bucket (min and max)
    hdr.xdelta = static_cast<double>(frames_per_point) / sample_rate / (output_complex ? 2.0 : 1.0);
    hdr.data_size = time_domain_out.size() * sizeof(float);
    
    // Write out bluefile
    hdr.type = 1000; // Type 1000 is for 1D generic data (used by SigPlot)
    
    std::vector<uint8_t> out_buffer;
    write_bluefile_header_mem(out_buffer, hdr);
    
    const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(time_domain_out.data());
    out_buffer.insert(out_buffer.end(), data_ptr, data_ptr + (time_domain_out.size() * sizeof(float)));
    
    spdlog::info("Time domain envelope successfully generated in memory");
    return out_buffer;
}

std::vector<uint8_t> DspTimeDomain::generate_time_domain_envelope(
    const std::string& input_file,
    double start_time,
    double duration,
    size_t target_points,
    const std::string& mode
) {
    BlueHeader hdr = read_bluefile_header(input_file);
    
    char data_type = hdr.format[1];
    double norm_factor = 1.0;
    if (data_type == 'B') norm_factor = 128.0;
    else if (data_type == 'I') norm_factor = 32768.0;
    else if (data_type == 'L') norm_factor = 2147483648.0;

    if (data_type == 'B') {
        return time_domain_data<int8_t>(input_file, start_time, duration, target_points, norm_factor, mode);
    } else if (data_type == 'I') {
        return time_domain_data<int16_t>(input_file, start_time, duration, target_points, norm_factor, mode);
    } else if (data_type == 'L') {
        return time_domain_data<int32_t>(input_file, start_time, duration, target_points, norm_factor, mode);
    } else if (data_type == 'F') {
        return time_domain_data<float>(input_file, start_time, duration, target_points, norm_factor, mode);
    } else if (data_type == 'D') {
        return time_domain_data<double>(input_file, start_time, duration, target_points, norm_factor, mode);
    } else {
        throw std::runtime_error("Unsupported data format type: " + std::string(1, data_type));
    }
}
std::vector<uint8_t> DspTimeDomain::extract_raw_iq(
    const std::string& input_file,
    double start_time,
    double duration,
    size_t max_points
) {
    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);
    
    size_t data_offset = static_cast<size_t>(hdr.data_start);
    size_t file_size = mmap_in.size;
    size_t data_size = file_size > data_offset ? file_size - data_offset : 0;
    
    bool is_complex = (hdr.format[0] == 'C');
    if (!is_complex) {
        throw std::runtime_error("extract_raw_iq requires complex input data.");
    }
    
    size_t bytes_per_sample = is_complex ? 8 : 4; // assuming floats
    if (hdr.format[1] == 'D') bytes_per_sample = is_complex ? 16 : 8;
    else if (hdr.format[1] == 'B') bytes_per_sample = is_complex ? 2 : 1;
    else if (hdr.format[1] == 'I') bytes_per_sample = is_complex ? 4 : 2;
    else if (hdr.format[1] == 'L') bytes_per_sample = is_complex ? 8 : 4;
    
    size_t total_samples = data_size / bytes_per_sample;
    
    double xdelta = hdr.xdelta > 0 ? hdr.xdelta : 1.0;
    
    size_t start_idx = static_cast<size_t>(start_time / xdelta);
    if (start_idx > total_samples) start_idx = total_samples;
    
    size_t end_idx = total_samples;
    if (duration > 0.0) {
        size_t dur_idx = static_cast<size_t>(duration / xdelta);
        end_idx = start_idx + dur_idx;
        if (end_idx > total_samples) end_idx = total_samples;
    }
    
    size_t samples_to_read = end_idx - start_idx;
    if (samples_to_read == 0) {
        return std::vector<uint8_t>();
    }
    
    // Decimation factor to limit points
    size_t step = 1;
    if (samples_to_read > max_points) {
        step = samples_to_read / max_points;
        samples_to_read = max_points;
    }
    
    std::vector<uint8_t> out_buffer;
    out_buffer.resize(samples_to_read * 8); // 8 bytes per complex point (2 floats)
    float* out_ptr = reinterpret_cast<float*>(out_buffer.data());
    
    const uint8_t* in_data = mmap_in.ptr + data_offset;
    
    for (size_t i = 0; i < samples_to_read; ++i) {
        size_t src_idx = start_idx + i * step;
        float i_val = 0.0f, q_val = 0.0f;
        
        if (hdr.format[1] == 'F') {
            const float* src = reinterpret_cast<const float*>(in_data + src_idx * 8);
            i_val = src[0];
            q_val = src[1];
        } else if (hdr.format[1] == 'D') {
            const double* src = reinterpret_cast<const double*>(in_data + src_idx * 16);
            i_val = static_cast<float>(src[0]);
            q_val = static_cast<float>(src[1]);
        } else if (hdr.format[1] == 'I') {
            const int16_t* src = reinterpret_cast<const int16_t*>(in_data + src_idx * 4);
            i_val = static_cast<float>(src[0]);
            q_val = static_cast<float>(src[1]);
        } else if (hdr.format[1] == 'B') {
            const int8_t* src = reinterpret_cast<const int8_t*>(in_data + src_idx * 2);
            i_val = static_cast<float>(src[0]);
            q_val = static_cast<float>(src[1]);
        } else if (hdr.format[1] == 'L') {
            const int32_t* src = reinterpret_cast<const int32_t*>(in_data + src_idx * 8);
            i_val = static_cast<float>(src[0]);
            q_val = static_cast<float>(src[1]);
        }
        
        out_ptr[i * 2] = i_val;
        out_ptr[i * 2 + 1] = q_val;
    }
    
    return out_buffer;
}
