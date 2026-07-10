#pragma once

#include <vector>
#include <string>
#include <complex>
#include <cmath>
#include <kfr/all.hpp>
#include "../bluefile_io.hpp"

namespace test_utils {

template <typename T>
void write_bluefile(const std::string& filename, const std::vector<T>& data, double sample_rate, bool is_complex) {
    BlueHeader hdr = {};
    std::strncpy(hdr.version, "BLUE", 4);
    std::strncpy(hdr.head_rep, "IEEE", 4);
    std::strncpy(hdr.data_rep, "IEEE", 4);
    hdr.type = 1000;
    
    if (is_complex) {
        if constexpr (std::is_same_v<T, float>) std::strncpy(hdr.format, "CF", 2);
        else if constexpr (std::is_same_v<T, double>) std::strncpy(hdr.format, "CD", 2);
        else if constexpr (std::is_same_v<T, int16_t>) std::strncpy(hdr.format, "CI", 2);
        else std::strncpy(hdr.format, "CF", 2);
    } else {
        if constexpr (std::is_same_v<T, float>) std::strncpy(hdr.format, "SF", 2);
        else if constexpr (std::is_same_v<T, double>) std::strncpy(hdr.format, "SD", 2);
        else if constexpr (std::is_same_v<T, int16_t>) std::strncpy(hdr.format, "SI", 2);
        else std::strncpy(hdr.format, "SF", 2);
    }
    
    hdr.xstart = 0.0;
    hdr.xdelta = 1.0 / sample_rate;
    hdr.xunits = 1; // Seconds
    
    hdr.data_start = 512.0;
    hdr.data_size = data.size() * sizeof(T);
    
    write_bluefile_header(filename, hdr);
    
    int fd = open(filename.c_str(), O_WRONLY | O_APPEND);
    if (fd >= 0) {
        write(fd, data.data(), data.size() * sizeof(T));
        close(fd);
    }
}

inline void generate_sine_bluefile(const std::string& filename, double sample_rate, double freq, size_t num_samples) {
    std::vector<float> data(num_samples * 2);
    for (size_t i = 0; i < num_samples; ++i) {
        double t = i / sample_rate;
        double phase = 2.0 * M_PI * freq * t;
        data[i * 2] = std::cos(phase);
        data[i * 2 + 1] = std::sin(phase);
    }
    write_bluefile(filename, data, sample_rate, true);
}

inline void generate_interference_bluefile(const std::string& filename, double sample_rate, size_t num_samples) {
    std::vector<float> data(num_samples * 2);
    for (size_t i = 0; i < num_samples; ++i) {
        double t = i / sample_rate;
        // White noise
        double noise_re = (static_cast<double>(std::rand()) / RAND_MAX) * 2.0 - 1.0;
        double noise_im = (static_cast<double>(std::rand()) / RAND_MAX) * 2.0 - 1.0;
        
        // CW tone
        double phase = 2.0 * M_PI * (sample_rate * 0.1) * t;
        double cw_re = std::cos(phase) * 10.0;
        double cw_im = std::sin(phase) * 10.0;
        
        // Pulse every 1000 samples for 10 samples
        double pulse = 0.0;
        if (i % 1000 < 10) pulse = 100.0;
        
        data[i * 2] = static_cast<float>(noise_re * 0.1 + cw_re + pulse);
        data[i * 2 + 1] = static_cast<float>(noise_im * 0.1 + cw_im + pulse);
    }
    write_bluefile(filename, data, sample_rate, true);
}

inline std::vector<float> read_bluefile_data(const std::string& filename) {
    MmapHandle mmap_in(filename);
    BlueHeader hdr = read_bluefile_header(filename);
    size_t num_elements = hdr.data_size / sizeof(float);
    std::vector<float> data(num_elements);
    std::memcpy(data.data(), mmap_in.ptr + static_cast<size_t>(hdr.data_start), hdr.data_size);
    return data;
}

} // namespace test_utils
