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
        int fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0) throw std::runtime_error("Cannot open BLUE file: " + filename);
        BlueHeader hdr;
        if (read(fd, &hdr, sizeof(BlueHeader)) != sizeof(BlueHeader)) {
            close(fd);
            throw std::runtime_error("Invalid BLUE file size");
        }
        close(fd);
        
        if (strncmp(hdr.version, "BLUE", 4) != 0) {
            throw std::runtime_error("Not a valid BLUE file");
        }
        
        if (hdr.type != 1000) {
            throw std::runtime_error("Only Type 1000 BLUE files are supported");
        }
        
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

struct MmapHandle {
    int fd = -1;
    uint8_t* ptr = nullptr;
    size_t size = 0;
    
    MmapHandle(const std::string& filename) {
        fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1) throw std::runtime_error("Could not open file for mmap: " + filename);
        
        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            throw std::runtime_error("Could not stat file: " + filename);
        }
        size = sb.st_size;
        
        ptr = static_cast<uint8_t*>(mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (ptr == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("mmap failed for file: " + filename);
        }
    }
    
    ~MmapHandle() {
        if (ptr && ptr != MAP_FAILED) munmap(ptr, size);
        if (fd != -1) close(fd);
    }
};

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
    
    // Required FFT size to get config.output_width bins across zoom_bw
    // bins_across_zoom = (zoom_bw / file_sample_rate) * fft_size
    // So fft_size = bins_across_zoom * file_sample_rate / zoom_bw
    double required_fft_size_d = static_cast<double>(config.output_width) * file_sample_rate / zoom_bw;
    
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
    
    if (channels == 2) {
        // Complex FFT: frequencies from -SR/2 to SR/2
        // We shift the output so it's linear from -SR/2 to SR/2.
        // We want to extract around `zoom_center` within `zoom_bw`.
        double start_freq = zoom_center - config.center_freq * 1e6 - zoom_bw / 2.0;
        
        // Map to bin index in shifted FFT
        extract_start_bin = static_cast<int>((start_freq + file_sample_rate / 2.0) / file_sample_rate * fft_size);
    } else {
        // Real FFT: frequencies from 0 to SR/2
        double start_freq = zoom_center - config.center_freq * 1e6 - zoom_bw / 2.0;
        if (start_freq < 0) start_freq = 0;
        extract_start_bin = static_cast<int>(start_freq / file_sample_rate * fft_size);
    }
    
    if (extract_start_bin < 0) extract_start_bin = 0;
    int bins_to_extract = static_cast<int>(zoom_bw / file_sample_rate * fft_size);
    if (bins_to_extract <= 0) bins_to_extract = 1;
    
    int max_bin = static_cast<int>(fft_size - bins_to_extract);
    if (extract_start_bin > max_bin) extract_start_bin = std::max(0, max_bin);
    if (extract_start_bin + bins_to_extract > static_cast<int>(fft_size)) {
        bins_to_extract = fft_size - extract_start_bin;
    }
    
    // Calculate actual bounds extracted
    double actual_start_freq = 0.0;
    if (channels == 2) {
        actual_start_freq = (static_cast<double>(extract_start_bin) / fft_size) * file_sample_rate - file_sample_rate / 2.0;
    } else {
        actual_start_freq = (static_cast<double>(extract_start_bin) / fft_size) * file_sample_rate;
    }
    
    result.actual_zoom_bw = (static_cast<double>(bins_to_extract) / fft_size) * file_sample_rate / 1e6;
    result.actual_zoom_center = (actual_start_freq + (result.actual_zoom_bw * 1e6) / 2.0) / 1e6 + config.center_freq;
    
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
    
    spdlog::info("Streaming: [Start {:.2f}s, End {:.2f}s] FFT Size: {}, Extract Bin: {}, Bins: {}, Rows: {}", 
                 result.original_start_time, end_sample / file_sample_rate, fft_size, extract_start_bin, bins_to_extract, num_rows);
    
    #pragma omp parallel
    {
        kfr::dft_plan<double>* plan_complex = nullptr;
        kfr::dft_plan_real<double>* plan_real = nullptr;
        
        if (channels == 2) plan_complex = new kfr::dft_plan<double>(fft_size);
        else plan_real = new kfr::dft_plan_real<double>(fft_size);
        
        std::vector<kfr::u8> temp_buffer(channels == 2 ? plan_complex->temp_size : plan_real->temp_size);
        kfr::univector<double> in_real(fft_size);
        kfr::univector<kfr::complex<double>> in_complex(fft_size);
        kfr::univector<kfr::complex<double>> out_complex(fft_size);
        
        #pragma omp for schedule(dynamic)
        for (size_t r = 0; r < num_rows; ++r) {
            size_t sample_idx = start_sample + r * step_size;
            
            // Average over config.time_smoothing
            std::vector<double> avg_mag(fft_size, 0.0);
            int valid_averages = 0;
            
            size_t sub_step = (config.time_smoothing > 1) ? (step_size / config.time_smoothing) : step_size;
            if (sub_step == 0) sub_step = 1;
            
            for (int a = 0; a < config.time_smoothing; ++a) {
                size_t a_start = sample_idx + a * sub_step;
                if (a_start + fft_size > end_sample) break;
                
                size_t byte_offset = data_offset + a_start * channels * (bits_per_sample / 8);
                
                // Read & Convert
                if (bits_per_sample == 8) {
                    if (config.is_wav) {
                        const uint8_t* ptr = mmap_file.ptr + byte_offset;
                        for (size_t i = 0; i < fft_size; ++i) {
                            if (channels == 2) in_complex[i] = kfr::complex<double>((ptr[2*i] - 127.5)/128.0, (ptr[2*i+1] - 127.5)/128.0) * window[i];
                            else in_real[i] = ((ptr[i] - 127.5)/128.0) * window[i];
                        }
                    } else {
                        const int8_t* ptr = reinterpret_cast<const int8_t*>(mmap_file.ptr + byte_offset);
                        for (size_t i = 0; i < fft_size; ++i) {
                            if (channels == 2) in_complex[i] = kfr::complex<double>(ptr[2*i]/128.0, ptr[2*i+1]/128.0) * window[i];
                            else in_real[i] = (ptr[i]/128.0) * window[i];
                        }
                    }
                } else if (bits_per_sample == 16) {
                    const int16_t* ptr = reinterpret_cast<const int16_t*>(mmap_file.ptr + byte_offset);
                    for (size_t i = 0; i < fft_size; ++i) {
                        if (channels == 2) in_complex[i] = kfr::complex<double>(ptr[2*i]/32768.0, ptr[2*i+1]/32768.0) * window[i];
                        else in_real[i] = (ptr[i]/32768.0) * window[i];
                    }
                } else if (bits_per_sample == 32) {
                    const float* ptr = reinterpret_cast<const float*>(mmap_file.ptr + byte_offset);
                    for (size_t i = 0; i < fft_size; ++i) {
                        if (channels == 2) in_complex[i] = kfr::complex<double>(ptr[2*i], ptr[2*i+1]) * window[i];
                        else in_real[i] = ptr[i] * window[i];
                    }
                } else if (bits_per_sample == 64) {
                    const double* ptr = reinterpret_cast<const double*>(mmap_file.ptr + byte_offset);
                    for (size_t i = 0; i < fft_size; ++i) {
                        if (channels == 2) in_complex[i] = kfr::complex<double>(ptr[2*i], ptr[2*i+1]) * window[i];
                        else in_real[i] = ptr[i] * window[i];
                    }
                }
                
                // Execute FFT
                if (channels == 2) {
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
        
        delete plan_complex;
        delete plan_real;
    }
    
    return result;
}
