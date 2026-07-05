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
void resample_data(const std::string& input_file, const std::string& output_file, double new_rate, int64_t interp_factor, int64_t dec_factor, resample_quality quality) {
    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);
    
    double old_rate = (hdr.xdelta > 0.0) ? (1.0 / hdr.xdelta) : 1.0;
    
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
    
    if (new_rate == 0.0) {
        new_rate = old_rate * static_cast<double>(interp_factor) / static_cast<double>(dec_factor);
    } else {
        // Automatically determine interp and dec factors from rates.
        // We MUST use std::round, because 1.0 / 3.333333333333334e-07 is 2999999.9999999995.
        // A simple static_cast truncates it to 2999999, completely destroying the GCD fraction!
        interp_factor = static_cast<int64_t>(std::round(new_rate));
        dec_factor = static_cast<int64_t>(std::round(old_rate));
    }
    
    // Simplify the fraction for logging
    int64_t gcd = std::gcd(interp_factor, dec_factor);
    int64_t log_interp = interp_factor / gcd;
    int64_t log_dec = dec_factor / gcd;
    
    samplerate_converter<fbase> resampler(quality, interp_factor, dec_factor);
    
    // Log detailed properties before starting
    spdlog::info("--- Resampling Properties ---");
    spdlog::info("Input rate:          {} Hz", old_rate);
    spdlog::info("Output rate:         {} Hz", new_rate);
    spdlog::info("Data format:         {} (complex={})", hdr.format, is_complex);
    spdlog::info("Input frames:        {}", num_frames);
    spdlog::info("Interpolation:       {}", log_interp);
    spdlog::info("Decimation:          {}", log_dec);
    spdlog::info("Quality level:       {}", static_cast<int>(quality));
    spdlog::info("Filter taps:         {}", resampler.filter_order(quality));
    spdlog::info("-----------------------------");
    
    size_t out_num_frames = static_cast<size_t>(std::round(num_frames * (new_rate / old_rate)));
    
    // Calculate output delay to flush the filter
    double delay_samples = static_cast<double>(resampler.get_delay());
    size_t out_delay = static_cast<size_t>(std::round(delay_samples * new_rate / old_rate));
    size_t total_out_frames = out_num_frames + out_delay;
    
    // We will allocate output buffers and run the resampler
    // KFR samplerate_converter processes floating point data. We need to cast if T is int.
    univector<fbase> in_f(num_frames);
    univector<fbase> out_f(total_out_frames); 
    
    std::vector<T> out_final(total_out_frames * samples_per_frame);
    
    if (is_complex) {
        // Resample I and Q independently
        univector<fbase> in_q(num_frames);
        for (size_t i = 0; i < num_frames; ++i) {
            in_f[i] = static_cast<fbase>(in_ptr[i * 2]);
            in_q[i] = static_cast<fbase>(in_ptr[i * 2 + 1]);
        }
        
        univector<fbase> out_q(out_f.size());
        
        samplerate_converter<fbase> resampler_q(quality, interp_factor, dec_factor);
        
        size_t processed_i = 0;
        size_t processed_q = 0;
        
        #pragma omp parallel sections
        {
            #pragma omp section
            {
                resampler.process(out_f, in_f);
            }
            #pragma omp section
            {
                resampler_q.process(out_q, in_q);
            }
        }
        
        for (size_t i = 0; i < total_out_frames; ++i) {
            out_final[i * 2] = static_cast<T>(out_f[i]);
            out_final[i * 2 + 1] = static_cast<T>(out_q[i]);
        }
        out_num_frames = total_out_frames;
    } else {
        for (size_t i = 0; i < num_frames; ++i) {
            in_f[i] = static_cast<fbase>(in_ptr[i]);
        }
        resampler.process(out_f, in_f);
        for (size_t i = 0; i < total_out_frames; ++i) {
            out_final[i] = static_cast<T>(out_f[i]);
        }
        out_num_frames = total_out_frames;
    }
    
    // Update header
    double delay_sec = delay_samples / old_rate;
    
    spdlog::info("--- Offset Correction ---");
    spdlog::info("Filter delay:        {} input samples", delay_samples);
    spdlog::info("Timecode shift:      {} seconds", delay_sec);
    
    if (hdr.timecode != 0.0) {
        spdlog::info("Original timecode:   {:.9f}", hdr.timecode);
        hdr.timecode -= delay_sec;
        spdlog::info("Corrected timecode:  {:.9f}", hdr.timecode);
    } else {
        spdlog::info("Original timecode is 0.0, skipping correction.");
    }
    
    hdr.xdelta = 1.0 / new_rate;
    hdr.data_size = out_num_frames * samples_per_frame * sizeof(T);
    
    spdlog::info("--- Output Properties ---");
    spdlog::info("Output frames:       {}", out_num_frames);
    spdlog::info("Output data size:    {} bytes", hdr.data_size);
    spdlog::info("-------------------------");
    
    std::vector<uint8_t> ext_data = read_bluefile_ext_header(input_file, hdr);
    if (!ext_data.empty()) {
        hdr.ext_start = static_cast<int32_t>(hdr.data_start + hdr.data_size);
    }
    
    spdlog::info("Resampler delay: {} samples ({} sec)", resampler.get_delay(), delay_sec);
    spdlog::info("Output frames: {}", out_num_frames);
    
    write_bluefile_header(output_file, hdr);
    
    // Append data
    int out_fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
    if (out_fd < 0) throw std::runtime_error("Could not open output file for appending");
    
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
    
    spdlog::info("Resampling complete.");
}

int main(int argc, char** argv) {
    CLI::App app{"DSP Resampler for X-Midas Bluefiles\n"
                 "High-performance tool to resample (interpolate and decimate) real and complex bluefiles.\n"
                 "Automatically updates the timecode (in seconds) to compensate for filter group delay."};
    
    std::string input_file;
    std::string output_file;
    double new_rate = 0.0;
    int64_t interp_factor = 1;
    int64_t dec_factor = 1;
    std::string quality_str = "normal";
    
    app.add_option("-i,--input", input_file, "Input Bluefile (.prm)")->required()->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_file, "Output Bluefile (.prm)")->required();
    app.add_option("-r,--rate", new_rate, "Target Output Sample Rate (Hz). If provided, interpolation and decimation factors are calculated automatically.");
    app.add_option("-I,--interpolation", interp_factor, "Manual interpolation factor (multiplier)");
    app.add_option("-d,--decimation", dec_factor, "Manual decimation factor (divisor)");
    app.add_option("-q,--quality", quality_str, "Resampling Filter Quality (draft, low, normal, high, perfect)");
    
    CLI11_PARSE(app, argc, argv);
    
    if (new_rate == 0.0 && (interp_factor == 1 && dec_factor == 1)) {
        spdlog::error("You must specify either --rate OR both --interpolation and --decimation.");
        return 1;
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
        if (type == 'B') resample_data<int8_t>(input_file, output_file, new_rate, interp_factor, dec_factor, quality);
        else if (type == 'I') resample_data<int16_t>(input_file, output_file, new_rate, interp_factor, dec_factor, quality);
        else if (type == 'L') resample_data<int32_t>(input_file, output_file, new_rate, interp_factor, dec_factor, quality);
        else if (type == 'F') resample_data<float>(input_file, output_file, new_rate, interp_factor, dec_factor, quality);
        else if (type == 'D') resample_data<double>(input_file, output_file, new_rate, interp_factor, dec_factor, quality);
        else throw std::runtime_error("Unsupported data type format");
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}
