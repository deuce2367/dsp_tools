#pragma once

#include <string>

class DspPower {
public:
    static void generate_power_envelope(
        const std::string& input_file,
        const std::string& output_file,
        double start_time = 0.0,
        double duration = 0.0,
        size_t target_points = 10000
    );
};
