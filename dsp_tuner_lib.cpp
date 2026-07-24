#include "dsp_tuner.hpp"
#include "bluefile_io.hpp"
#include <kfr/base.hpp>
#include <kfr/dsp.hpp>
#include <kfr/dft.hpp>
#include <kfr/dft/convolution.hpp>
#include <kfr/io.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <chrono>
#include <iostream>
#include <cmath>
#include <numeric>

#ifdef _OPENMP
#include <omp.h>
#endif

using namespace kfr;

template<typename T>
void tune_data(const std::string& input_file, const std::string& output_file, double center, double bandwidth, double start_time, double duration, double file_center, bool file_center_provided, resample_quality quality, int oversample_factor) {
    auto exec_start_time = std::chrono::high_resolution_clock::now();
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
    
    double shift_freq = file_center - center;
    
    // Automatic optimal sample rate using integer decimation
    int64_t dec_factor = static_cast<int64_t>(std::floor(sample_rate / bandwidth));
    if (dec_factor < 1) dec_factor = 1;
    
    int64_t interp_factor = 1;
    if (oversample_factor > 1) {
        if (dec_factor % oversample_factor == 0) {
            dec_factor /= oversample_factor;
        } else {
            int64_t gcd = std::gcd(dec_factor, static_cast<int64_t>(oversample_factor));
            dec_factor /= gcd;
            interp_factor = oversample_factor / gcd;
        }
    }
    
    if (dec_factor > 1024) dec_factor = 1024; // Cap decimation to prevent massive FIR filters
    
    double output_rate = sample_rate * static_cast<double>(interp_factor) / static_cast<double>(dec_factor);
    
    int64_t gcd = std::gcd(interp_factor, dec_factor);
    int64_t log_interp = interp_factor / gcd;
    int64_t log_dec = dec_factor / gcd;
    
    spdlog::info("--- Tuner Properties ---");
    spdlog::info("Input file:          {}", input_file);
    spdlog::info("Input format:        {} (complex={})", hdr.format, is_complex);
    spdlog::info("Input rate:          {} Hz", sample_rate);
    spdlog::info("Input frames:        {}", num_frames);
    spdlog::info("Start time offset:   {} sec ({} frames skipped)", start_time, frames_to_skip);
    spdlog::info("Duration:            {} sec ({} frames processed)", duration, num_frames);
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
    char out_format[2];
    out_format[0] = 'C';
    out_format[1] = hdr.format[1];
    
    hdr.format[0] = out_format[0];
    hdr.format[1] = out_format[1];
    hdr.xdelta = 1.0 / output_rate;
    
    size_t out_num_frames = static_cast<size_t>(std::round(num_frames * (output_rate / sample_rate)));
    size_t out_delay = static_cast<size_t>(std::round(delay_samples * output_rate / sample_rate));
    size_t total_out_frames = out_num_frames + out_delay;
    
    spdlog::info("--- Output Properties ---");
    spdlog::info("Output format:       {}", hdr.format);
    spdlog::info("Output rate:         {} Hz", output_rate);
    
    if (hdr.timecode != 0.0) {
        spdlog::info("Original timecode:   {:.9f}", hdr.timecode);
        hdr.timecode += start_time;
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
    
    univector<fbase> mix_i(num_frames);
    univector<fbase> mix_q(num_frames);
    
    fbase phase_step = std::fmod(shift_freq / sample_rate, 1.0);
    
    #pragma omp parallel
    {
#ifdef _OPENMP
        int thread_id = omp_get_thread_num();
        int num_threads = omp_get_num_threads();
#else
        int thread_id = 0;
        int num_threads = 1;
#endif
        
        size_t chunk_size = (num_frames + num_threads - 1) / num_threads;
        size_t start = thread_id * chunk_size;
        size_t end = std::min(start + chunk_size, num_frames);
        
        if (start < end) {
            double p_step = phase_step * 2.0 * M_PI;
            double step_cos = std::cos(p_step);
            double step_sin = std::sin(p_step);
            
            double p_start = std::fmod(start * phase_step, 1.0) * 2.0 * M_PI;
            double current_cos = std::cos(p_start);
            double current_sin = std::sin(p_start);
            
            for (size_t i = start; i < end; ++i) {
                fbase nco_i = current_cos;
                fbase nco_q = current_sin;
                
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
                
                // Advance
                double next_c = current_cos * step_cos - current_sin * step_sin;
                double next_s = current_sin * step_cos + current_cos * step_sin;
                current_cos = next_c;
                current_sin = next_s;
                
                // Re-normalize periodically inside large chunks to prevent drift
                if (i % 1048576 == 0 && i > start) {
                    double p = std::fmod(i * phase_step, 1.0) * 2.0 * M_PI;
                    current_cos = std::cos(p);
                    current_sin = std::sin(p);
                }
            }
        }
    }
    
    if (oversample_factor > 1) {
        fbase f_cutoff = (bandwidth / 2.0) / sample_rate;
        size_t taps = 1023; // High quality FIR filter
        univector<fbase> lpf_taps(taps);
        fir_lowpass(lpf_taps, f_cutoff, to_handle(window_blackman_harris(taps)), true);
        
        univector<fbase> lpf_taps_copy = lpf_taps;
        convolve_filter<fbase> filter_i(lpf_taps, 4096);
        convolve_filter<fbase> filter_q(lpf_taps_copy, 4096);
        
        univector<fbase> filtered_i(num_frames);
        univector<fbase> filtered_q(num_frames);
        
        const size_t block_size = 1048576; // 1M frames per block
        size_t frames_processed = 0;
        
        while (frames_processed < num_frames) {
            size_t frames_to_process = std::min(block_size, num_frames - frames_processed);
            
            auto in_i_ref = mix_i.slice(frames_processed, frames_to_process);
            auto out_i_ref = filtered_i.slice(frames_processed, frames_to_process);
            
            auto in_q_ref = mix_q.slice(frames_processed, frames_to_process);
            auto out_q_ref = filtered_q.slice(frames_processed, frames_to_process);
            
            #pragma omp parallel sections
            {
                #pragma omp section
                {
                    filter_i.apply(out_i_ref, in_i_ref);
                }
                #pragma omp section
                {
                    filter_q.apply(out_q_ref, in_q_ref);
                }
            }
            
            frames_processed += frames_to_process;
        }
        
        mix_i = std::move(filtered_i);
        mix_q = std::move(filtered_q);
        
        double lpf_delay_samples = taps / 2;
        double lpf_delay_sec = lpf_delay_samples / sample_rate;
        
        // Output header timecode already advanced by resampler delay, add LPF delay
        hdr.timecode -= lpf_delay_sec;
        spdlog::info("Added FIR LPF delay: {} samples ({} sec). Corrected timecode: {:.9f}", lpf_delay_samples, lpf_delay_sec, hdr.timecode);
    }
    
    // Pad buffers to avoid KFR SIMD overruns and handle slight length variations
    univector<fbase> out_f_i(total_out_frames + 4096);
    univector<fbase> out_f_q(total_out_frames + 4096);
    
    size_t processed_i = 0, processed_q = 0;
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            processed_i = resampler_i.process(out_f_i, mix_i);
        }
        #pragma omp section
        {
            processed_q = resampler_q.process(out_f_q, mix_q);
        }
    }
    
    size_t actual_out_frames = std::min({total_out_frames, processed_i, processed_q});
    std::vector<T> out_final(actual_out_frames * out_samples_per_frame);
    
    #pragma omp parallel for
    for (size_t i = 0; i < actual_out_frames; ++i) {
        out_final[i * 2] = static_cast<T>(out_f_i[i]);
        out_final[i * 2 + 1] = static_cast<T>(out_f_q[i]);
    }

    
    const uint8_t* out_bytes = reinterpret_cast<const uint8_t*>(out_final.data());
    size_t to_write = std::min<size_t>(hdr.data_size, actual_out_frames * out_samples_per_frame * sizeof(T));
    size_t written = 0;
    while (written < to_write) {
        ssize_t res = write(out_fd, out_bytes + written, to_write - written);
        if (res <= 0) {
            close(out_fd);
            throw std::runtime_error("Failed writing data");
        }
        written += res;
    }
    close(out_fd);
    
    write_bluefile_ext_header(output_file, ext_data);
    
    auto exec_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = exec_end_time - exec_start_time;
    spdlog::info("Tuning complete. Runtime: {:.3f} seconds", diff.count());
}

void run_tuner_pipeline(
    const std::string& input_file, 
    const std::string& output_file, 
    double center, 
    double bandwidth,
    double start_time,
    double duration,
    double file_center, 
    bool file_center_provided, 
    TunerQuality quality_enum,
    int oversample_factor
) {
    resample_quality quality = resample_quality::normal;
    switch (quality_enum) {
        case TunerQuality::Draft: quality = resample_quality::draft; break;
        case TunerQuality::Low: quality = resample_quality::low; break;
        case TunerQuality::Normal: quality = resample_quality::normal; break;
        case TunerQuality::High: quality = resample_quality::high; break;
        case TunerQuality::Perfect: quality = resample_quality::perfect; break;
    }

    auto exec_start_time = std::chrono::high_resolution_clock::now();
    BlueHeader hdr = read_bluefile_header(input_file);
    char type = hdr.format[1];
    
    if (type == 'B') tune_data<int8_t>(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality, oversample_factor);
    else if (type == 'I') tune_data<int16_t>(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality, oversample_factor);
    else if (type == 'L') tune_data<int32_t>(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality, oversample_factor);
    else if (type == 'F') tune_data<float>(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality, oversample_factor);
    else if (type == 'D') tune_data<double>(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality, oversample_factor);
    else throw std::runtime_error("Unsupported data type format");
}
