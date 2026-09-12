#include "CLI/CLI.hpp"
#include <spdlog/spdlog.h>
#include <string>

extern void demodulate_pipeline(const std::string& input_file, const std::string& output_wav, double target_freq, double bandwidth, double audio_rate, const std::string& demod_type);

int main(int argc, char** argv) {
    CLI::App app{"DSP Demodulator with Liquid-DSP"};

    std::string input_file;
    std::string output_wav;
    double target_freq = 0.0;
    double bandwidth = 0.0;
    double audio_rate = 48000.0;
    std::string demod_type = "FM";

    app.add_option("-i,--input", input_file, "Input .prm file (CF or CI)")->required();
    app.add_option("-o,--output", output_wav, "Output .wav file")->required();
    app.add_option("-f,--freq", target_freq, "Target center frequency (Hz)");
    app.add_option("-b,--bw", bandwidth, "Target bandwidth (Hz) (Currently unused in lib but reserved)");
    app.add_option("-r,--rate", audio_rate, "Target audio output rate (Hz, default 48000)");
    app.add_option("-t,--type", demod_type, "Demodulation type (FM, AM, default FM)");
    if (argc == 1) {
        std::cout << app.help() << std::endl;
        return 0;
    }
    
    CLI11_PARSE(app, argc, argv);

    demodulate_pipeline(input_file, output_wav, target_freq, bandwidth, audio_rate, demod_type);

    return 0;
}
