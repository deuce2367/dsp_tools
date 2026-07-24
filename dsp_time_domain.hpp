#pragma once

#include <string>
#include <cstdint>
#include <cstddef>
#include <vector>

class DspTimeDomain {
public:
    static std::vector<uint8_t> generate_time_domain_envelope(
        const std::string& input_file,
        double start_time = 0.0,
        double duration = 0.0, // 0.0 means read to end
        size_t target_points = 10000,
        const std::string& mode = "complex"
    );

    static std::vector<uint8_t> extract_raw_iq(
        const std::string& input_file,
        double start_time = 0.0,
        double duration = 0.0,
        size_t max_points = 100000
    );
};
