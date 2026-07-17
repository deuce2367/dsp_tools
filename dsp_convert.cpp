#include "dsp_convert.hpp"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>

int main(int argc, char** argv) {
    CLI::App app{"DSP Convert: Imports WAV and RAW files into X-Midas Bluefiles"};
    
    std::string input_file;
    std::string output_file;
    std::string format = "";
    double rate = 0.0;
    double freq_mhz = 0.0;
    double timecode = 0.0;
    bool sigmf = false;
    
    app.add_option("-i,--input", input_file, "Input file (.wav, .raw, .dat)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    app.add_option("--format", format, "Data format for RAW (e.g., SB, SI, SF, CB, CI, CF)");
    app.add_option("--rate", rate, "Sample rate for RAW in Hz (default: 1.0)");
    app.add_option("--freq", freq_mhz, "Center frequency in MHz (default: 0.0)");
    app.add_option("--timecode", timecode, "Start time (J1950 seconds) (default: 0.0)");
    app.add_flag("--sigmf", sigmf, "Generate a sidecar .sigmf-meta JSON file");
    
    CLI11_PARSE(app, argc, argv);
    
    try {
        run_convert_pipeline(input_file, output_file, format, rate, freq_mhz, sigmf, timecode);
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
