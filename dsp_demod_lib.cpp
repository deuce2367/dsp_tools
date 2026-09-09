#include <kfr/dft.hpp>
#include <kfr/dsp.hpp>
#include <kfr/io.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include <cmath>
#include <complex>
#include <liquid/liquid.h>
#include "bluefile_io.hpp"

using namespace kfr;

void demodulate_pipeline(const std::string& input_file, const std::string& output_wav, double target_freq, double bandwidth, double audio_rate, const std::string& demod_type) {
    spdlog::info("Demodulation pipeline started for {}", input_file);
    spdlog::info("Target Freq: {} Hz, BW: {} Hz, Audio Rate: {} Hz, Type: {}", target_freq, bandwidth, audio_rate, demod_type);

    BlueHeader hdr = read_bluefile_header(input_file);
    MmapHandle mmap_in(input_file);

    double input_rate = (hdr.xdelta > 0.0) ? (1.0 / hdr.xdelta) : 1000000.0;
    bool is_complex = (hdr.format[0] == 'C');

    if (!is_complex) {
        spdlog::error("Demodulator requires complex input data");
        return;
    }

    double file_center = 0.0;
    std::string keyword_str(hdr.keywords, strnlen(hdr.keywords, sizeof(hdr.keywords)));
    size_t rf_pos = keyword_str.find("RF_FREQUENCY_MHZ=");
    if (rf_pos != std::string::npos) {
        size_t val_start = rf_pos + 17;
        size_t val_end = keyword_str.find_first_of(";\n ", val_start);
        if (val_end == std::string::npos) val_end = keyword_str.length();
        try {
            file_center = std::stod(keyword_str.substr(val_start, val_end - val_start)) * 1e6; // Convert MHz to Hz
        } catch(...) {}
    }

    // Setup Tuner
    double shift_freq = file_center - target_freq;
    double phase_step = -2.0 * M_PI * shift_freq / input_rate;
    double current_phase = 0.0;


    // Setup Resampler (Decimator + Anti-Alias)
    int64_t iq_dec_factor = 1;
    if (demod_type == "FM" || demod_type == "fm") {
        iq_dec_factor = std::max(1L, (int64_t)std::round(input_rate / 250000.0));
    } else {
        iq_dec_factor = std::max(1L, (int64_t)std::round(input_rate / audio_rate));
    }
    double actual_if_rate = input_rate / iq_dec_factor;
    kfr::samplerate_converter<float> resamp_i(kfr::resample_quality::normal, 1, iq_dec_factor);
    kfr::samplerate_converter<float> resamp_q(kfr::resample_quality::normal, 1, iq_dec_factor);
    
    // For audio decimation after demod (from actual_if_rate to audio_rate)
    int64_t audio_interp = 1000;
    int64_t audio_dec = std::max(1L, (int64_t)std::round(actual_if_rate / audio_rate * 1000.0));
    kfr::samplerate_converter<float> resamp_audio(kfr::resample_quality::normal, audio_interp, audio_dec);
    double actual_audio_rate = actual_if_rate * audio_interp / audio_dec;


    // Setup Demodulator (Liquid-DSP)
    freqdem fm_demod = nullptr;
    ampmodem am_demod = nullptr;
    float fm_deviation = 75000.0f;

    if (demod_type == "WFM" || demod_type == "wfm" || demod_type == "FM" || demod_type == "fm") {
        fm_deviation = 75000.0f;
        float kf = fm_deviation / actual_audio_rate;
        fm_demod = freqdem_create(kf);
    } else if (demod_type == "NFM" || demod_type == "nfm") {
        fm_deviation = 5000.0f;
        float kf = fm_deviation / actual_audio_rate;
        fm_demod = freqdem_create(kf);
    } else if (demod_type == "AM" || demod_type == "am") {
        am_demod = ampmodem_create(0.5f, LIQUID_AMPMODEM_DSB, 0);
    } else if (demod_type == "USB" || demod_type == "usb") {
        am_demod = ampmodem_create(0.5f, LIQUID_AMPMODEM_USB, 0);
    } else if (demod_type == "LSB" || demod_type == "lsb") {
        am_demod = ampmodem_create(0.5f, LIQUID_AMPMODEM_LSB, 0);
    } else {
        spdlog::error("Unknown demod type: {}", demod_type);
        return;
    }

    // Audio Writer
    kfr::audio_format audio_fmt;
    audio_fmt.channels = 2; // Output standard identical stereo
    audio_fmt.type = kfr::audio_sample_type::f32;
    audio_fmt.samplerate = actual_audio_rate;
    kfr::audio_writer_wav<float> wav_writer(kfr::open_file_for_writing(output_wav), audio_fmt);

    size_t data_offset = static_cast<size_t>(hdr.data_start);
    size_t data_bytes = static_cast<size_t>(hdr.data_size);

    if (data_offset + data_bytes > mmap_in.size) {
        if (fm_demod) freqdem_destroy(fm_demod);
        if (am_demod) ampmodem_destroy(am_demod);
        spdlog::error("Invalid data_size in bluefile header");
        return;
    }

    bool is_float = (hdr.format[1] == 'F');
    bool is_int16 = (hdr.format[1] == 'I');
    
    if (!is_float && !is_int16) {
        if (fm_demod) freqdem_destroy(fm_demod);
        if (am_demod) ampmodem_destroy(am_demod);
        spdlog::error("Unsupported format type: {}", hdr.format[1]);
        return;
    }

    size_t total_samples = data_bytes / (is_float ? sizeof(float) : sizeof(int16_t));
    size_t total_input_frames = total_samples / 2;

    const float* in_ptr_f32 = reinterpret_cast<const float*>(mmap_in.ptr + data_offset);
    const int16_t* in_ptr_i16 = reinterpret_cast<const int16_t*>(mmap_in.ptr + data_offset);

    size_t frames_processed = 0;
    size_t chunk_size = 102400;
    size_t total_written_frames = 0;

    std::vector<kfr::complex<float>> in_buffer(chunk_size);
    std::vector<float> in_i(chunk_size);
    std::vector<float> in_q(chunk_size);

    // FM Deemphasis filter (75us time constant)
    float deemphasis_alpha = 1.0f - std::exp(-1.0f / (actual_if_rate * 75e-6f));
    float deemphasis_prev = 0.0f;

    while (frames_processed < total_input_frames) {
        size_t frames_to_process = std::min(chunk_size, total_input_frames - frames_processed);
        
        for (size_t i = 0; i < frames_to_process; ++i) {
            float i_val, q_val;
            if (is_float) {
                i_val = in_ptr_f32[(frames_processed + i) * 2];
                q_val = in_ptr_f32[(frames_processed + i) * 2 + 1];
            } else {
                i_val = in_ptr_i16[(frames_processed + i) * 2] / 32768.0f;
                q_val = in_ptr_i16[(frames_processed + i) * 2 + 1] / 32768.0f;
            }
            
            // 1. Tune (NCO shift)
            kfr::complex<float> samp(i_val, q_val);
            samp *= kfr::complex<float>(std::cos(current_phase), std::sin(current_phase));
            current_phase += phase_step;
            in_i[i] = samp.real();
            in_q[i] = samp.imag();
        }

        // Normalize phase
        while (current_phase > M_PI) current_phase -= 2.0 * M_PI;
        while (current_phase < -M_PI) current_phase += 2.0 * M_PI;

        // 2. Resample / Decimate
        kfr::univector_ref<float> unvec_in_i(in_i.data(), frames_to_process);
        kfr::univector_ref<float> unvec_in_q(in_q.data(), frames_to_process);

        size_t expected_out_frames = frames_to_process / iq_dec_factor;
        kfr::univector<float> out_i(expected_out_frames);
        kfr::univector<float> out_q(expected_out_frames);

        resamp_i.process(out_i, unvec_in_i);
        resamp_q.process(out_q, unvec_in_q);
        
        size_t out_frames = expected_out_frames;


        // 3. Demodulate
        std::vector<float> demod_out(out_frames);
        if (fm_demod) {
            for (size_t i = 0; i < out_frames; ++i) {
                float complex_samp[2] = {out_i[i], out_q[i]};
                float samp = 0.0f;
                freqdem_demodulate(fm_demod, *(liquid_float_complex*)complex_samp, &samp);
                
                // Volume adjust since deviation max is fm_deviation
                // delta_phase = 2 * pi * fm_deviation / actual_if_rate
                // So max samp should be around this delta_phase
                float max_dev = 2.0f * M_PI * fm_deviation / actual_if_rate;
                samp = samp / max_dev; // Normalize to roughly [-1, 1]
                
                // Apply deemphasis filter
                samp = deemphasis_prev + deemphasis_alpha * (samp - deemphasis_prev);
                deemphasis_prev = samp;
                
                demod_out[i] = samp;
            }
        } else if (am_demod) {
            for (size_t i = 0; i < out_frames; ++i) {
                float complex_samp[2] = {out_i[i], out_q[i]};
                float samp = 0.0f;
                ampmodem_demodulate(am_demod, *(liquid_float_complex*)complex_samp, &samp);
                demod_out[i] = samp * 2.0f;
            }
        }

        // 4. Resample audio to final rate
        kfr::univector_ref<float> unvec_demod_out(demod_out.data(), out_frames);
        size_t expected_final_frames = (out_frames * audio_interp) / audio_dec + 10;
        kfr::univector<float> final_audio(expected_final_frames);
        resamp_audio.process(final_audio, unvec_demod_out);
        
        size_t final_frames = final_audio.size();
        
        std::vector<float> audio_stereo(final_frames * 2);
        for (size_t i = 0; i < final_frames; ++i) {
            float samp = final_audio[i];
            // Hard clip to avoid wrapping/distortion
            if (samp > 0.99f) samp = 0.99f;
            if (samp < -0.99f) samp = -0.99f;
            audio_stereo[i * 2] = samp;
            audio_stereo[i * 2 + 1] = samp;
        }

        // 5. Write to disk
        if (final_frames > 0) {
            wav_writer.write(audio_stereo.data(), final_frames);
            total_written_frames += final_frames;
        }


        frames_processed += frames_to_process;
    }

    if (fm_demod) freqdem_destroy(fm_demod);
    if (am_demod) ampmodem_destroy(am_demod);

    spdlog::info("Demodulation pipeline completed. Wrote {} audio frames.", total_written_frames);
}
