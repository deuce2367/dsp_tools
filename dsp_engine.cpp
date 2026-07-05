#include "dsp_engine.hpp"
#include <kfr/math.hpp>
#include <kfr/dft.hpp>
#include <kfr/io/audiofile.hpp>
#include <kfr/io/file.hpp>
#include <kfr/dsp/window.hpp>
#include <spdlog/spdlog.h>
#include <cmath>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <algorithm>

DspEngine::DspEngine(size_t fft_size) : m_fft_size(fft_size) {}
DspEngine::~DspEngine() {}
std::vector<double> DspEngine::compute_fft_magnitude_db(const std::vector<double>& real_samples) {
    kfr::dft_plan_real<double> plan(m_fft_size);
    std::vector<kfr::u8> temp(plan.temp_size);
    
    kfr::univector<double> in(m_fft_size);
    for (size_t i = 0; i < std::min(m_fft_size, real_samples.size()); ++i) {
        in[i] = real_samples[i];
    }
    // Pad with zeros if needed
    for (size_t i = real_samples.size(); i < m_fft_size; ++i) {
        in[i] = 0.0;
    }
    
    kfr::univector<kfr::complex<double>> out(m_fft_size / 2 + 1);
    
    plan.execute(out, in, temp.data());
    
    std::vector<double> mag_db(m_fft_size / 2 + 1);
    for (size_t i = 0; i < mag_db.size(); ++i) {
        double magnitude = kfr::cabs(out[i]);
        // Avoid log of zero
        if (magnitude < 1e-10) magnitude = 1e-10;
        mag_db[i] = 20.0 * std::log10(magnitude);
    }
    
    return mag_db;
}
void DspEngine::get_file_info(const std::string& filename, int& channels, double& sample_rate, bool& is_wav, bool& is_blue, std::string& format_str, double& timecode) {
    is_wav = false;
    is_blue = false;
    format_str = "float32";
    timecode = 0.0;

    if (filename.size() >= 4) {
        std::string ext = filename.substr(filename.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".wav") is_wav = true;
        if (ext == ".prm" || ext == ".tmp") is_blue = true;
    }

    if (is_wav) {
        auto reader = kfr::audio_reader_wav<double>(kfr::open_file_for_reading(filename));
        const kfr::audio_format_and_length& fmt = reader.format();
        channels = fmt.channels;
        sample_rate = fmt.samplerate;
    } else if (is_blue) {
        BlueHeader hdr = read_bluefile_header(filename);
        
        char f_size = hdr.format[0];
        char f_type = hdr.format[1];
        
        if (f_size == 'C') channels = 2;
        else if (f_size == 'S') channels = 1;
        else throw std::runtime_error(std::string("Unsupported BLUE format size: ") + f_size);
        
        if (f_type == 'B') format_str = "int8";
        else if (f_type == 'I') format_str = "int16";
        else if (f_type == 'L') format_str = "int32";
        else if (f_type == 'F') format_str = "float32";
        else if (f_type == 'D') format_str = "float64";
        else throw std::runtime_error(std::string("Unsupported BLUE format type: ") + f_type);
        
        if (hdr.xdelta > 0.0) {
            sample_rate = 1.0 / hdr.xdelta;
        } else {
            sample_rate = 1.0;
        }
        timecode = hdr.timecode;
    }
}



DspEngine::StreamingResult DspEngine::process_file_streaming(const StreamConfig& config) {
    StreamingResult result;
    
    // 1. Memory Map the file
    MmapHandle mmap_file(config.filename);
    
    size_t data_offset = 0;
    size_t total_samples = 0;
    int channels = 1;
    int bits_per_sample = 32;
    double file_sample_rate = config.sample_rate;
    
    // 2. Parse Headers
    if (config.is_wav) {
        if (mmap_file.size < 44) throw std::runtime_error("WAV file too small");
        // Simple WAV parsing (assuming standard format)
        uint32_t chunk_size;
        memcpy(&chunk_size, mmap_file.ptr + 4, 4);
        
        uint16_t audio_format;
        memcpy(&audio_format, mmap_file.ptr + 20, 2);
        
        uint16_t num_channels;
        memcpy(&num_channels, mmap_file.ptr + 22, 2);
        channels = num_channels;
        
        uint32_t sr;
        memcpy(&sr, mmap_file.ptr + 24, 4);
        file_sample_rate = sr;
        
        uint16_t bps;
        memcpy(&bps, mmap_file.ptr + 34, 2);
        bits_per_sample = bps;
        
        // Find data chunk
        size_t offset = 12; // skip RIFF/WAVE
        while (offset + 8 < mmap_file.size) {
            std::string chunk_id(reinterpret_cast<char*>(mmap_file.ptr + offset), 4);
            uint32_t csize;
            memcpy(&csize, mmap_file.ptr + offset + 4, 4);
            if (chunk_id == "data") {
                data_offset = offset + 8;
                total_samples = csize / (channels * (bps / 8));
                break;
            }
            offset += 8 + csize;
        }
    } else if (config.is_blue) {
        if (mmap_file.size < sizeof(BlueHeader)) throw std::runtime_error("BLUE file too small");
        BlueHeader* hdr = reinterpret_cast<BlueHeader*>(mmap_file.ptr);
        
        data_offset = static_cast<size_t>(hdr->data_start);
        if (data_offset == 0) data_offset = 512; // default
        
        char f_size = hdr->format[0];
        char f_type = hdr->format[1];
        
        if (f_size == 'C') channels = 2;
        else if (f_size == 'S') channels = 1;
        
        if (f_type == 'B') bits_per_sample = 8;
        else if (f_type == 'I') bits_per_sample = 16;
        else if (f_type == 'L' || f_type == 'F') bits_per_sample = 32;
        else if (f_type == 'D') bits_per_sample = 64;
        
        if (hdr->xdelta > 0.0) file_sample_rate = 1.0 / hdr->xdelta;
        
        size_t dsize = static_cast<size_t>(hdr->data_size);
        if (dsize == 0 || data_offset + dsize > mmap_file.size) {
            dsize = mmap_file.size - data_offset;
        }
        total_samples = dsize / (channels * (bits_per_sample / 8));
    } else {
        // Raw file
        data_offset = 0;
        if (config.data_type == "float32" || config.data_type == "complex") {
            bits_per_sample = 32;
        } else if (config.data_type == "int16") {
            bits_per_sample = 16;
        } else if (config.data_type == "int8") {
            bits_per_sample = 8;
        }
        if (config.data_type == "complex") channels = 2;
        else channels = 1;
        
        total_samples = mmap_file.size / (channels * (bits_per_sample / 8));
    }
    
    // 3. Time Slicing
    size_t start_sample = 0;
    if (config.start_time > 0.0) {
        start_sample = static_cast<size_t>(config.start_time * file_sample_rate);
    }
    size_t end_sample = total_samples;
    if (config.end_time > 0.0) {
        end_sample = static_cast<size_t>(config.end_time * file_sample_rate);
    }
    if (start_sample >= total_samples) start_sample = 0;
    if (end_sample > total_samples || end_sample <= start_sample) end_sample = total_samples;
    
    result.original_start_time = config.original_timecode + (start_sample / file_sample_rate);
    size_t active_samples = end_sample - start_sample;
    
    // 4. Frequency Zooming
    double file_bw = file_sample_rate;
    if (channels == 1) file_bw = file_sample_rate / 2.0; // Real signals only have positive frequencies
    
    double zoom_bw = config.zoom_bw > 0.0 ? config.zoom_bw * 1e6 : file_bw;
    double zoom_center = config.zoom_center * 1e6;
    
    // Constrain zoom_bw
    if (zoom_bw > file_bw) zoom_bw = file_bw;
    
    int decimation_factor = 1;
    double f_shift = 0.0;
    double new_sample_rate = file_sample_rate;
    
    // Determine if we can channelize (zoom)
    if (config.zoom_bw > 0.0) {
        // Find integer decimation factor, leaving 30% margin for anti-alias filter rolloff
        decimation_factor = static_cast<int>(file_sample_rate / (config.zoom_bw * 1e6 * 1.3));
        if (decimation_factor < 1) decimation_factor = 1;
        if (decimation_factor > 1000) decimation_factor = 1000;
        
        if (decimation_factor > 1) {
            new_sample_rate = file_sample_rate / decimation_factor;
            if (channels == 2) {
                f_shift = (config.zoom_center - config.center_freq) * 1e6;
            } else {
                double baseband_zero_hz_mapped = config.center_freq * 1e6 - file_sample_rate / 4.0;
                f_shift = config.zoom_center * 1e6 - baseband_zero_hz_mapped;
            }
            zoom_center = 0.0; // The signal is shifted to baseband
        }
    }
    
    double required_fft_size_d = static_cast<double>(config.output_width) * new_sample_rate / zoom_bw;
    
    // We want the next power of two
    size_t base_fft = std::max<size_t>(1024, kfr::next_poweroftwo(static_cast<size_t>(required_fft_size_d)));
    
    // If the required FFT size is insanely large (e.g., > 131072), we clamp it to avoid OOM/slowness
    // and just let the output be lower resolution.
    size_t max_fft_size = 131072;
    size_t fft_size = config.window_size > 0 ? config.window_size : std::min<size_t>(base_fft, max_fft_size);
    
    // Calculate the bin range to extract
    // freq for bin k = (k / fft_size) * file_sample_rate
    // for complex, freq goes from -SR/2 to SR/2
    // bin 0 is 0 Hz, bin N/2 is SR/2, bin N/2+1 is -SR/2
    
    int extract_start_bin = 0;
    
    if (channels == 2 || decimation_factor > 1) {
        // Complex FFT: frequencies from -SR/2 to SR/2
        // We shift the output so it's linear from -SR/2 to SR/2.
        // We want to extract around `zoom_center` within `zoom_bw`.
        double start_freq = zoom_center - zoom_bw / 2.0;
        
        if (decimation_factor == 1) {
            start_freq = zoom_center - config.center_freq * 1e6 - zoom_bw / 2.0;
        }
        
        // Map to bin index in shifted FFT
        extract_start_bin = static_cast<int>((start_freq + new_sample_rate / 2.0) / new_sample_rate * fft_size);
    } else {
        // Real FFT: frequencies from 0 to SR/2.
        // We map the physical center of the band (SR/4) to config.center_freq.
        // So 0 Hz maps to config.center_freq - SR/4.
        double baseband_zero_hz_mapped = config.center_freq * 1e6 - new_sample_rate / 4.0;
        double start_freq = zoom_center - baseband_zero_hz_mapped - zoom_bw / 2.0;
        if (start_freq < 0) start_freq = 0;
        extract_start_bin = static_cast<int>(start_freq / new_sample_rate * fft_size);
    }
    
    if (extract_start_bin < 0) extract_start_bin = 0;
    int bins_to_extract = static_cast<int>(zoom_bw / new_sample_rate * fft_size);
    if (bins_to_extract <= 0) bins_to_extract = 1;
    
    int max_bin = static_cast<int>(fft_size - bins_to_extract);
    if (extract_start_bin > max_bin) extract_start_bin = std::max(0, max_bin);
    if (extract_start_bin + bins_to_extract > static_cast<int>(fft_size)) {
        bins_to_extract = fft_size - extract_start_bin;
    }
    
    // Calculate actual bounds extracted
    result.actual_zoom_bw = (static_cast<double>(bins_to_extract) / fft_size) * new_sample_rate / 1e6;
    
    double actual_start_freq = 0.0;
    if (channels == 2 || decimation_factor > 1) {
        actual_start_freq = (static_cast<double>(extract_start_bin) / fft_size) * new_sample_rate - new_sample_rate / 2.0;
        result.actual_zoom_center = (actual_start_freq + (result.actual_zoom_bw * 1e6) / 2.0) / 1e6 + (decimation_factor > 1 ? config.zoom_center : config.center_freq);
    } else {
        actual_start_freq = (static_cast<double>(extract_start_bin) / fft_size) * new_sample_rate;
        double baseband_zero_hz_mapped = config.center_freq * 1e6 - new_sample_rate / 4.0;
        result.actual_zoom_center = (actual_start_freq + (result.actual_zoom_bw * 1e6) / 2.0 + baseband_zero_hz_mapped) / 1e6;
    }
    
    // 5. Setup Window
    kfr::univector<double> window(fft_size);
    if (config.window_type == "hann") window = kfr::window_hann(fft_size);
    else if (config.window_type == "hamming") window = kfr::window_hamming(fft_size);
    else if (config.window_type == "flattop") window = kfr::window_flattop(fft_size);
    else if (config.window_type == "bartlett") window = kfr::window_bartlett(fft_size);
    else window = kfr::window_blackman_harris(fft_size);
    
    double window_sum = 0.0;
    for (size_t i = 0; i < fft_size; ++i) window_sum += window[i];
    
    // 6. Waterfall Generation
    size_t step_size = config.step_size;
    if (step_size == 0) {
        if (active_samples > fft_size) {
            step_size = (active_samples - fft_size) / config.output_height;
        }
        if (step_size == 0) step_size = 1;
    }
    result.actual_step_size = step_size;
    
    size_t num_rows = std::min<size_t>(config.output_height, (active_samples) / step_size);
    if (num_rows == 0) num_rows = 1;
    
    result.spectrogram.resize(num_rows);
    for (auto& row : result.spectrogram) {
        row.resize(bins_to_extract, -160.0); // fill with tiny dB initially
    }
    result.first_frame_mag.resize(bins_to_extract, -160.0);
    result.avg_fft.resize(bins_to_extract, -160.0);
    result.max_hold_fft.resize(bins_to_extract, -1000.0);
    result.min_hold_fft.resize(bins_to_extract, 1000.0);
    
    spdlog::info("Streaming: [Start {:.2f}s, End {:.2f}s] FFT Size: {}, Extract Bin: {}, Bins: {}, Rows: {}", 
                 result.original_start_time, end_sample / file_sample_rate, fft_size, extract_start_bin, bins_to_extract, num_rows);
    
    #pragma omp parallel
    {
        kfr::dft_plan<double>* plan_complex = nullptr;
        kfr::dft_plan_real<double>* plan_real = nullptr;
        
        if (channels == 2 || decimation_factor > 1) plan_complex = new kfr::dft_plan<double>(fft_size);
        else plan_real = new kfr::dft_plan_real<double>(fft_size);
        
        std::vector<kfr::u8> temp_buffer((channels == 2 || decimation_factor > 1) ? plan_complex->temp_size : plan_real->temp_size);
        size_t in_size_needed = fft_size * decimation_factor;
        int num_taps = 64;
        if (decimation_factor > 1) in_size_needed += num_taps;
        
        kfr::univector<kfr::complex<double>> in_complex(fft_size);
        kfr::univector<double> in_real(fft_size);
        kfr::univector<kfr::complex<double>> out_complex(fft_size);
        
        // FIR filter taps
        std::vector<double> taps(num_taps);
        if (decimation_factor > 1) {
            double fc = 0.45 / decimation_factor;
            for (int i = 0; i < num_taps; ++i) {
                double n = i - (num_taps - 1) / 2.0;
                if (n == 0) taps[i] = 2.0 * fc;
                else taps[i] = sin(2.0 * M_PI * fc * n) / (M_PI * n);
                taps[i] *= 0.42 - 0.5 * cos(2.0 * M_PI * i / (num_taps - 1)) + 0.08 * cos(4.0 * M_PI * i / (num_taps - 1));
            }
            double sum = 0;
            for (double t : taps) sum += t;
            for (double& t : taps) t /= sum;
        }
        
        size_t sub_step = (config.time_smoothing > 1) ? (step_size / config.time_smoothing) : step_size;
        if (sub_step == 0) sub_step = 1;
        
        size_t baseband_sub_step = sub_step;
        size_t max_baseband_samples_needed = fft_size;
        size_t max_raw_samples_needed = fft_size;
        
        if (decimation_factor > 1) {
            baseband_sub_step = std::max<size_t>(1, sub_step / decimation_factor);
            max_baseband_samples_needed = fft_size + (config.time_smoothing - 1) * baseband_sub_step;
            max_raw_samples_needed = max_baseband_samples_needed * decimation_factor + num_taps;
        } else {
            max_raw_samples_needed = fft_size + (config.time_smoothing - 1) * sub_step;
        }
        
        std::vector<kfr::complex<double>> thread_raw_samples(max_raw_samples_needed);
        std::vector<kfr::complex<double>> thread_baseband_samples(decimation_factor > 1 ? max_baseband_samples_needed : 0);
        
        #pragma omp for schedule(dynamic)
        for (size_t r = 0; r < num_rows; ++r) {
            size_t sample_idx = start_sample + r * step_size;
            
            // Average over config.time_smoothing
            std::vector<double> avg_mag(fft_size, 0.0);
            int valid_averages = 0;
            
            if (sample_idx > end_sample) continue;
            
            size_t raw_samples_to_read = std::min(max_raw_samples_needed, end_sample - sample_idx);
            
            auto read_sample = [&](size_t idx, double& r_val, double& i_val) {
                size_t boffset = data_offset + (sample_idx + idx) * channels * (bits_per_sample / 8);
                if (bits_per_sample == 8) {
                    if (config.is_wav) {
                        const uint8_t* ptr = mmap_file.ptr + boffset;
                        r_val = (ptr[0] - 127.5)/128.0; if (channels == 2) i_val = (ptr[1] - 127.5)/128.0;
                    } else {
                        const int8_t* ptr = reinterpret_cast<const int8_t*>(mmap_file.ptr + boffset);
                        r_val = ptr[0]/128.0; if (channels == 2) i_val = ptr[1]/128.0;
                    }
                } else if (bits_per_sample == 16) {
                    const int16_t* ptr = reinterpret_cast<const int16_t*>(mmap_file.ptr + boffset);
                    r_val = ptr[0]/32768.0; if (channels == 2) i_val = ptr[1]/32768.0;
                } else if (bits_per_sample == 32) {
                    const float* ptr = reinterpret_cast<const float*>(mmap_file.ptr + boffset);
                    r_val = ptr[0]; if (channels == 2) i_val = ptr[1];
                } else if (bits_per_sample == 64) {
                    const double* ptr = reinterpret_cast<const double*>(mmap_file.ptr + boffset);
                    r_val = ptr[0]; if (channels == 2) i_val = ptr[1];
                }
            };

            if (decimation_factor > 1) {
                // Grab samples for decimation
                double f_shift_norm = f_shift / file_sample_rate;
                double start_phase = -2.0 * M_PI * f_shift_norm * sample_idx; // absolute phase sync
                kfr::complex<double> phasor(cos(-2.0 * M_PI * f_shift_norm), sin(-2.0 * M_PI * f_shift_norm));
                kfr::complex<double> current_phase_val(cos(start_phase), sin(start_phase));
                
                for (size_t i = 0; i < raw_samples_to_read; ++i) {
                    double sr = 0, si = 0;
                    read_sample(i, sr, si);
                    thread_raw_samples[i] = kfr::complex<double>(sr, si) * current_phase_val;
                    current_phase_val = current_phase_val * phasor;
                    if (i % 1000 == 0) { // normalize
                        double mag = std::sqrt(current_phase_val.real() * current_phase_val.real() + current_phase_val.imag() * current_phase_val.imag());
                        current_phase_val = current_phase_val / mag;
                    }
                }
                for (size_t i = raw_samples_to_read; i < max_raw_samples_needed; ++i) thread_raw_samples[i] = 0;
                
                // Decimate the entire block ONCE
                for (size_t i = 0; i < max_baseband_samples_needed; ++i) {
                    kfr::complex<double> sum = 0;
                    size_t base_idx = i * decimation_factor;
                    if (base_idx + num_taps <= max_raw_samples_needed) {
                        for (int k = 0; k < num_taps; ++k) {
                            sum += thread_raw_samples[base_idx + k] * taps[k];
                        }
                    }
                    thread_baseband_samples[i] = sum;
                }
            } else {
                for (size_t i = 0; i < raw_samples_to_read; ++i) {
                    double sr = 0, si = 0;
                    read_sample(i, sr, si);
                    thread_raw_samples[i] = kfr::complex<double>(sr, si);
                }
                for (size_t i = raw_samples_to_read; i < max_raw_samples_needed; ++i) thread_raw_samples[i] = 0;
            }

            for (int a = 0; a < config.time_smoothing; ++a) {
                size_t a_start = sample_idx + a * sub_step;
                if (a_start + fft_size > end_sample) break;
                
                if (decimation_factor > 1) {
                    size_t base = a * baseband_sub_step;
                    for (size_t i = 0; i < fft_size; ++i) {
                        if (base + i < max_baseband_samples_needed) {
                            in_complex[i] = thread_baseband_samples[base + i] * window[i];
                        } else {
                            in_complex[i] = 0;
                        }
                    }
                } else {
                    size_t base = a * sub_step;
                    for (size_t i = 0; i < fft_size; ++i) {
                        if (base + i < max_raw_samples_needed) {
                            if (channels == 2) in_complex[i] = thread_raw_samples[base + i] * window[i];
                            else in_real[i] = thread_raw_samples[base + i].real() * window[i];
                        } else {
                            if (channels == 2) in_complex[i] = 0; else in_real[i] = 0;
                        }
                    }
                }
                
                // Execute FFT
                if (channels == 2 || decimation_factor > 1) {
                    plan_complex->execute(out_complex, in_complex, temp_buffer.data());
                    // Shift so 0Hz is center
                    for (size_t i = 0; i < fft_size / 2; ++i) {
                        avg_mag[i + fft_size / 2] += kfr::cabs(out_complex[i]);
                        avg_mag[i] += kfr::cabs(out_complex[i + fft_size / 2]);
                    }
                } else {
                    plan_real->execute(out_complex, in_real, temp_buffer.data());
                    for (size_t i = 0; i < fft_size / 2 + 1; ++i) {
                        avg_mag[i] += kfr::cabs(out_complex[i]);
                    }
                }
                valid_averages++;
            }
            
            if (valid_averages > 0) {
                // Extract zoom window
                for (size_t i = 0; i < bins_to_extract; ++i) {
                    int bin = extract_start_bin + i;
                    if (bin >= 0 && bin < static_cast<int>(fft_size)) {
                        double mag = avg_mag[bin] / valid_averages;
                        mag /= window_sum; // Normalize by coherent gain
                        if (mag <= 1e-16) mag = 1e-16;
                        result.spectrogram[r][i] = 20.0 * std::log10(mag);
                    }
                }
            }
            
            if (r == 0) {
                for (size_t i = 0; i < bins_to_extract; ++i) {
                    result.first_frame_mag[i] = result.spectrogram[r][i];
                }
            }
        }
        
        // --- SECONDARY PASS: Calculate Max/Min Hold and Average ---
        // This is extremely fast and thread-safe because we process columns sequentially
        if (num_rows > 0) {
            for (size_t i = 0; i < bins_to_extract; ++i) {
                double sum_db = 0.0;
                double max_db = -1000.0;
                double min_db = 1000.0;
                
                for (size_t r = 0; r < num_rows; ++r) {
                    double val = result.spectrogram[r][i];
                    sum_db += val;
                    if (val > max_db) max_db = val;
                    if (val < min_db) min_db = val;
                }
                
                result.avg_fft[i] = sum_db / num_rows;
                result.max_hold_fft[i] = max_db;
                result.min_hold_fft[i] = min_db;
            }
        }
        
        delete plan_complex;
        delete plan_real;
    }
    
    return result;
}
