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
#include <numeric>

using namespace kfr;

#ifndef DSP_TOOLS_TEST_MODE
#include "dsp_tuner.hpp"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    CLI::App app{"DSP Tuner for X-Midas Bluefiles\n"
                 "Digital Down Converter (DDC). Tunes to a target center frequency (shifting it to 0 Hz baseband) and optimally decimates the signal.\n"
                 "The output sample rate is automatically set to the target bandwidth, which guarantees an optimal anti-aliasing lowpass filter.\n"
                 "Real input signals will automatically be converted to complex baseband outputs."};
    
    std::string input_file;
    std::string output_file;
    double center = 0.0;
    double bandwidth = 0.0;
    double start_time = 0.0;
    double duration = 0.0;
    double file_center = 0.0;
    bool file_center_provided = false;
    std::string quality_str = "normal";
    
    app.add_option("-i,--input", input_file, "Input Bluefile (.prm)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    app.add_option("-c,--center", center, "Target Center Frequency to tune to (Hz)")->required();
    app.add_option("-b,--bandwidth", bandwidth, "Target Bandwidth (Hz). The output sample rate will be automatically decimated to this value.")->required();
    
    app.add_option("--start", start_time, "Start time in seconds relative to file start (default 0.0)");
    app.add_option("--duration", duration, "Duration in seconds to process (default: until EOF)");
    
    auto opt_fc = app.add_option("-f,--file-center", file_center, "Original Center Frequency of the input file (Hz). Default: 0 for complex inputs, fs/4 for real inputs.");
    app.add_option("-q,--quality", quality_str, "Resampling Filter Quality (draft, low, normal, high, perfect). Higher quality yields steeper transition bands.");
    
    CLI11_PARSE(app, argc, argv);
    
    if (opt_fc->count() > 0) {
        file_center_provided = true;
    }
    
    TunerQuality quality = TunerQuality::Normal;
    if (quality_str == "draft") quality = TunerQuality::Draft;
    else if (quality_str == "low") quality = TunerQuality::Low;
    else if (quality_str == "normal") quality = TunerQuality::Normal;
    else if (quality_str == "high") quality = TunerQuality::High;
    else if (quality_str == "perfect") quality = TunerQuality::Perfect;
    else {
        spdlog::error("Invalid quality level.");
        return 1;
    }
    
    try {
        run_tuner_pipeline(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality);
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
#endif

