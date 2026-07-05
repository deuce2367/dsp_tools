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

template<typename T>
void tune_data(const std::string& input_file, const std::string& output_file, double center, double bandwidth, double file_center, bool file_center_provided, resample_quality quality) {
    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);
    
    double sample_rate = (hdr.xdelta > 0.0) ? (1.0 / hdr.xdelta) : 1.0;
    bool is_complex = (hdr.format[0] == 'C');
    
    // Set default file center if not provided
    if (!file_center_provided) {
        file_center = is_complex ? 0.0 : (sample_rate / 4.0);
    }
    
    size_t data_offset = static_cast<size_t>(hdr.data_start);
    size_t data_bytes = static_cast<size_t>(hdr.data_size);
    
    if (data_offset + data_bytes > mmap_in.size) {
        throw std::runtime_error("Invalid data_size in bluefile header");
    }
    
    T* in_ptr = reinterpret_cast<T*>(mmap_in.ptr + data_offset);
    size_t total_samples = data_bytes / sizeof(T);
    size_t samples_per_frame = is_complex ? 2 : 1;
    size_t num_frames = total_samples / samples_per_frame;
    
    double shift_freq = file_center - center;
    
    // Automatic optimal sample rate
    double output_rate = bandwidth;
    int64_t interp_factor = static_cast<int64_t>(std::round(output_rate));
    int64_t dec_factor = static_cast<int64_t>(std::round(sample_rate));
    int64_t gcd = std::gcd(interp_factor, dec_factor);
    int64_t log_interp = interp_factor / gcd;
    int64_t log_dec = dec_factor / gcd;
    
    spdlog::info("--- Tuner Properties ---");
    spdlog::info("Input file:          {}", input_file);
    spdlog::info("Input format:        {} (complex={})", hdr.format, is_complex);
    spdlog::info("Input rate:          {} Hz", sample_rate);
    spdlog::info("Input frames:        {}", num_frames);
    spdlog::info("File center:         {} Hz", file_center);
    spdlog::info("Target center:       {} Hz", center);
    spdlog::info("Shift frequency:     {} Hz", shift_freq);
    spdlog::info("Target bandwidth:    {} Hz", bandwidth);
    spdlog::info("Optimal output rate: {} Hz", output_rate);
    spdlog::info("Interpolation:       {}", log_interp);
    spdlog::info("Decimation:          {}", log_dec);
    
    samplerate_converter<fbase> resampler_i(quality, interp_factor, dec_factor);
    samplerate_converter<fbase> resampler_q(quality, interp_factor, dec_factor);
    
    double delay_samples = static_cast<double>(resampler_i.get_delay());
    double delay_sec = delay_samples / sample_rate;
    
    spdlog::info("Filter taps:         {}", resampler_i.filter_order(quality));
    spdlog::info("-------------------------");
    
    // Output is ALWAYS complex baseband (format CI, CD, CF, etc.)
    char out_format[4];
    out_format[0] = 'C';
    out_format[1] = hdr.format[1];
    out_format[2] = '\0';
    out_format[3] = '\0';
    
    hdr.format[0] = out_format[0];
    hdr.format[1] = out_format[1];
    hdr.format[2] = '\0';
    hdr.xdelta = 1.0 / output_rate;
    
    size_t out_num_frames = static_cast<size_t>(std::round(num_frames * (output_rate / sample_rate)));
    size_t out_delay = static_cast<size_t>(std::round(delay_samples * output_rate / sample_rate));
    size_t total_out_frames = out_num_frames + out_delay;
    
    spdlog::info("--- Output Properties ---");
    spdlog::info("Output format:       {}", hdr.format);
    spdlog::info("Output rate:         {} Hz", output_rate);
    if (hdr.timecode != 0.0) {
        spdlog::info("Original timecode:   {:.9f}", hdr.timecode);
        hdr.timecode -= delay_sec;
        spdlog::info("Corrected timecode:  {:.9f}", hdr.timecode);
    }
    
    std::vector<uint8_t> ext_data = read_bluefile_ext_header(input_file, hdr);
    
    size_t out_samples_per_frame = 2;
    hdr.data_size = out_num_frames * out_samples_per_frame * sizeof(T);
    if (!ext_data.empty()) {
        hdr.ext_start = static_cast<int32_t>(hdr.data_start + hdr.data_size);
    }
    
    spdlog::info("Resampler delay:     {} samples ({} sec)", resampler_i.get_delay(), delay_sec);
    spdlog::info("Output frames:       {}", out_num_frames);
    spdlog::info("-------------------------");
    
    write_bluefile_header(output_file, hdr);
    int out_fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
    if (out_fd < 0) throw std::runtime_error("Could not open output file for appending");
    
    // Load entire file (we have sufficient RAM based on previous steps) or chunk it?
    // Since resampler.process() needs to process sequential chunks, we can chunk it just like dsp_resample.
    // Wait, in dsp_resample we passed the entire vector. But here we mix first.
    // So we can mix everything into a large buffer, then resample.
    // If the file is 10M frames * 2 * 8 = 160MB. That easily fits in RAM.
    
    univector<fbase> mix_i(num_frames);
    univector<fbase> mix_q(num_frames);
    
    fbase phase_step = std::fmod(shift_freq / sample_rate, 1.0);
    
    #pragma omp parallel for
    for (size_t i = 0; i < num_frames; ++i) {
        fbase p = std::fmod(i * phase_step, 1.0);
        fbase nco_i = std::cos(p * 2.0 * M_PI);
        fbase nco_q = std::sin(p * 2.0 * M_PI);
        
        if (is_complex) {
            fbase ii = static_cast<fbase>(in_ptr[i * 2]);
            fbase qq = static_cast<fbase>(in_ptr[i * 2 + 1]);
            mix_i[i] = ii * nco_i - qq * nco_q;
            mix_q[i] = ii * nco_q + qq * nco_i;
        } else {
            fbase ii = static_cast<fbase>(in_ptr[i]);
            mix_i[i] = ii * nco_i;
            mix_q[i] = ii * nco_q;
        }
    }
    
    univector<fbase> out_f_i(total_out_frames);
    univector<fbase> out_f_q(total_out_frames);
    
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            resampler_i.process(out_f_i, mix_i);
        }
        #pragma omp section
        {
            resampler_q.process(out_f_q, mix_q);
        }
    }
    
    std::vector<T> out_final(total_out_frames * out_samples_per_frame);
    #pragma omp parallel for
    for (size_t i = 0; i < total_out_frames; ++i) {
        out_final[i * 2] = static_cast<T>(out_f_i[i]);
        out_final[i * 2 + 1] = static_cast<T>(out_f_q[i]);
    }
    
    const uint8_t* out_bytes = reinterpret_cast<const uint8_t*>(out_final.data());
    size_t to_write = hdr.data_size;
    size_t written = 0;
    while (written < to_write) {
        ssize_t res = write(out_fd, out_bytes + written, to_write - written);
        if (res < 0) {
            close(out_fd);
            throw std::runtime_error("Failed writing data");
        }
        written += res;
    }
    close(out_fd);
    
    write_bluefile_ext_header(output_file, ext_data);
    
    spdlog::info("Tuning complete.");
}

int main(int argc, char** argv) {
    CLI::App app{"DSP Tuner for X-Midas Bluefiles\n"
                 "Digital Down Converter (DDC). Tunes to a target center frequency (shifting it to 0 Hz baseband) and optimally decimates the signal.\n"
                 "The output sample rate is automatically set to the target bandwidth, which guarantees an optimal anti-aliasing lowpass filter.\n"
                 "Real input signals will automatically be converted to complex baseband outputs."};
    
    std::string input_file;
    std::string output_file;
    double center = 0.0;
    double bandwidth = 0.0;
    double file_center = 0.0;
    bool file_center_provided = false;
    std::string quality_str = "normal";
    
    app.add_option("-i,--input", input_file, "Input Bluefile (.prm)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    app.add_option("-c,--center", center, "Target Center Frequency to tune to (Hz)")->required();
    app.add_option("-b,--bandwidth", bandwidth, "Target Bandwidth (Hz). The output sample rate will be automatically decimated to this value.")->required();
    
    auto opt_fc = app.add_option("-f,--file-center", file_center, "Original Center Frequency of the input file (Hz). Default: 0 for complex inputs, fs/4 for real inputs.");
    app.add_option("-q,--quality", quality_str, "Resampling Filter Quality (draft, low, normal, high, perfect). Higher quality yields steeper transition bands.");
    
    CLI11_PARSE(app, argc, argv);
    
    if (opt_fc->count() > 0) {
        file_center_provided = true;
    }
    
    resample_quality quality = resample_quality::normal;
    if (quality_str == "draft") quality = resample_quality::draft;
    else if (quality_str == "low") quality = resample_quality::low;
    else if (quality_str == "normal") quality = resample_quality::normal;
    else if (quality_str == "high") quality = resample_quality::high;
    else if (quality_str == "perfect") quality = resample_quality::perfect;
    else {
        spdlog::error("Invalid quality level.");
        return 1;
    }
    
    try {
        BlueHeader hdr = read_bluefile_header(input_file);
        char type = hdr.format[1];
        if (type == 'B') tune_data<int8_t>(input_file, output_file, center, bandwidth, file_center, file_center_provided, quality);
        else if (type == 'I') tune_data<int16_t>(input_file, output_file, center, bandwidth, file_center, file_center_provided, quality);
        else if (type == 'L') tune_data<int32_t>(input_file, output_file, center, bandwidth, file_center, file_center_provided, quality);
        else if (type == 'F') tune_data<float>(input_file, output_file, center, bandwidth, file_center, file_center_provided, quality);
        else if (type == 'D') tune_data<double>(input_file, output_file, center, bandwidth, file_center, file_center_provided, quality);
        else throw std::runtime_error("Unsupported data type format");
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
