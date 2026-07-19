#include <iostream>
#include <CLI/CLI.hpp>
#include "dsp_power.hpp"

int main(int argc, char** argv) {
    CLI::App app{"DSP Power Envelope Generator"};

    std::string input_file;
    app.add_option("-i,--input", input_file, "Input Bluefile")->required()->check(CLI::ExistingFile);

    std::string output_file;
    app.add_option("-o,--output", output_file, "Output Bluefile")->required();

    double start_time = 0.0;
    app.add_option("-s,--start", start_time, "Start time in seconds");

    double duration = 0.0;
    app.add_option("-d,--duration", duration, "Duration to process in seconds");

    size_t target_points = 10000;
    app.add_option("-p,--points", target_points, "Target number of points for output envelope");

    CLI11_PARSE(app, argc, argv);

    try {
        DspPower::generate_power_envelope(
            input_file,
            output_file,
            start_time,
            duration,
            target_points
        );
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
