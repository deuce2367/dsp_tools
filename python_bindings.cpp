#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "dsp_engine.hpp"
#include "dsp_time_domain.hpp"
#include "plot_generator.hpp"
#include "bluefile_io.hpp"
#include "dsp_convert.hpp"
#include "dsp_tuner.hpp"
#include <ctime>
#include <chrono>
#include <iomanip>
#include <iomanip>
#include <sstream>
#include <cmath>

std::string format_timecode(double timecode) {
    if (timecode > 1e8) {
        time_t unix_time = static_cast<time_t>(timecode - 631152000.0);
        struct tm* tm_info = gmtime(&unix_time);
        if (tm_info) {
            char buffer[64];
            strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", tm_info);
            double intpart;
            double fractpart = modf(timecode, &intpart);
            if (fractpart > 0.001) {
                char frac_buf[16];
                snprintf(frac_buf, sizeof(frac_buf), "%.3f", fractpart);
                return std::string(buffer) + std::string(frac_buf).substr(1) + "Z";
            }
            return std::string(buffer) + "Z";
        }
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2fs", timecode);
    return std::string(buffer);
}

namespace py = pybind11;

PYBIND11_MODULE(dsp_plotter_py, m) {
    m.doc() = "DSP Plotter Python bindings";

    py::class_<DspEngine::StreamConfig>(m, "StreamConfig")
        .def(py::init<>())
        .def_readwrite("filename", &DspEngine::StreamConfig::filename)
        .def_readwrite("is_wav", &DspEngine::StreamConfig::is_wav)
        .def_readwrite("is_blue", &DspEngine::StreamConfig::is_blue)
        .def_readwrite("data_type", &DspEngine::StreamConfig::data_type)
        .def_readwrite("sample_rate", &DspEngine::StreamConfig::sample_rate)
        .def_readwrite("start_time", &DspEngine::StreamConfig::start_time)
        .def_readwrite("end_time", &DspEngine::StreamConfig::end_time)
        .def_readwrite("original_timecode", &DspEngine::StreamConfig::original_timecode)
        .def_readwrite("center_freq", &DspEngine::StreamConfig::center_freq)
        .def_readwrite("zoom_center", &DspEngine::StreamConfig::zoom_center)
        .def_readwrite("zoom_bw", &DspEngine::StreamConfig::zoom_bw)
        .def_readwrite("window_size", &DspEngine::StreamConfig::window_size)
        .def_readwrite("step_size", &DspEngine::StreamConfig::step_size)
        .def_readwrite("output_width", &DspEngine::StreamConfig::output_width)
        .def_readwrite("output_height", &DspEngine::StreamConfig::output_height)
        .def_readwrite("time_smoothing", &DspEngine::StreamConfig::time_smoothing)
        .def_readwrite("window_type", &DspEngine::StreamConfig::window_type);

    py::class_<DspEngine::StreamingResult>(m, "StreamingResult")
        .def_readonly("spectrogram", &DspEngine::StreamingResult::spectrogram)
        .def_readonly("first_frame_mag", &DspEngine::StreamingResult::first_frame_mag)
        .def_readonly("avg_fft", &DspEngine::StreamingResult::avg_fft)
        .def_readonly("max_hold_fft", &DspEngine::StreamingResult::max_hold_fft)
        .def_readonly("min_hold_fft", &DspEngine::StreamingResult::min_hold_fft)
        .def_readonly("actual_step_size", &DspEngine::StreamingResult::actual_step_size)
        .def_readonly("actual_zoom_center", &DspEngine::StreamingResult::actual_zoom_center)
        .def_readonly("actual_zoom_bw", &DspEngine::StreamingResult::actual_zoom_bw)
        .def_readonly("original_start_time", &DspEngine::StreamingResult::original_start_time);

    py::class_<DspEngine>(m, "DspEngine")
        .def(py::init<size_t>())
        .def("process_file_streaming", &DspEngine::process_file_streaming)
        .def("get_file_info", [](DspEngine& self, const std::string& filename) {
            int channels = 0;
            double sample_rate = 1.0;
            bool is_wav = false;
            bool is_blue = false;
            std::string format_str = "";
            double timecode = 0.0;
            double center_freq = 0.0;
            self.get_file_info(filename, channels, sample_rate, is_wav, is_blue, format_str, timecode, center_freq);
            return py::make_tuple(channels, sample_rate, is_wav, is_blue, format_str, timecode, center_freq);
        })
        .def_static("update_header", [](const std::string& filename, double timecode, double center_freq) {
            update_bluefile_header(filename, timecode, center_freq);
        });

    py::class_<PlotGenerator>(m, "PlotGenerator")
        .def_static("generate_fast_waterfall", &PlotGenerator::generate_fast_waterfall,
            py::arg("spectrogram_db"), py::arg("output_filename"), py::arg("out_width"), py::arg("out_height"),
            py::arg("colormap"), py::arg("min_db"), py::arg("max_db"), py::arg("center_freq_mhz"), py::arg("bandwidth_mhz"),
            py::arg("start_time_iso"), py::arg("total_duration_sec"), py::arg("draw_grid") = true, py::arg("draw_labels") = true,
            py::arg("out_format") = "png", py::arg("num_x_ticks") = 10, py::arg("num_y_ticks") = 10, py::arg("title") = "",
            py::arg("jpeg_quality") = 90, py::arg("png_compression") = 8, py::arg("font_path") = "",
            py::arg("box_start_time") = -1.0, py::arg("box_duration") = 0.0, py::arg("box_center_freq") = 0.0, py::arg("box_bw") = 0.0,
            py::arg("box_color") = "red")
        .def_static("generate_fast_fft_plot", &PlotGenerator::generate_fast_fft_plot,
            py::arg("frequency_bins"), py::arg("magnitude_db"), py::arg("output_filename"), py::arg("out_width"), py::arg("out_height"),
            py::arg("min_db"), py::arg("max_db"), py::arg("center_freq_mhz"), py::arg("bandwidth_mhz"),
            py::arg("draw_grid") = true, py::arg("draw_labels") = true, py::arg("out_format") = "png",
            py::arg("num_x_ticks") = 10, py::arg("num_y_ticks") = 10, py::arg("title") = "",
            py::arg("jpeg_quality") = 90, py::arg("png_compression") = 8, py::arg("colormap_name") = "jet", py::arg("font_path") = "");

    m.def("run_convert", [](const std::string& input_file, const std::string& output_file, 
                            const std::string& format, double rate, double freq_mhz, bool sigmf, double timecode) {
        run_convert_pipeline(input_file, output_file, format, rate, freq_mhz, sigmf, timecode);
    }, py::arg("input_file"), py::arg("output_file"), py::arg("format")="", py::arg("rate")=1.0, 
       py::arg("freq_mhz")=0.0, py::arg("sigmf")=false, py::arg("timecode")=0.0);

    m.def("run_tuner_pipeline", [](const std::string& input_file, const std::string& output_file, 
                                   double center, double bandwidth, double start_time, double duration, 
                                   double file_center, bool file_center_provided, const std::string& quality_str) {
        TunerQuality quality = TunerQuality::Normal;
        if (quality_str == "draft") quality = TunerQuality::Draft;
        else if (quality_str == "low") quality = TunerQuality::Low;
        else if (quality_str == "normal") quality = TunerQuality::Normal;
        else if (quality_str == "high") quality = TunerQuality::High;
        else if (quality_str == "perfect") quality = TunerQuality::Perfect;
        run_tuner_pipeline(input_file, output_file, center, bandwidth, start_time, duration, file_center, file_center_provided, quality);
    }, py::arg("input_file"), py::arg("output_file"), py::arg("center"), py::arg("bandwidth"), 
       py::arg("start_time")=0.0, py::arg("duration")=0.0, py::arg("file_center")=0.0, py::arg("file_center_provided")=false, 
       py::arg("quality_str")="normal", py::call_guard<py::gil_scoped_release>());

    
    m.def("run_time_domain", [](const std::string& input_file, double start_time, double duration, size_t target_points) {
        std::vector<uint8_t> out_buffer = DspTimeDomain::generate_time_domain_envelope(input_file, start_time, duration, target_points);
        std::string buffer(reinterpret_cast<const char*>(out_buffer.data()), out_buffer.size());
        return py::bytes(buffer);
    }, py::arg("input_file"), py::arg("start_time"), py::arg("duration"), py::arg("target_points"), py::call_guard<py::gil_scoped_release>());
    
    m.def("run_constellation_data", [](const std::string& input_file, double start_time, double duration, size_t max_points) {
        std::vector<uint8_t> raw_buffer = DspTimeDomain::extract_raw_iq(input_file, start_time, duration, max_points);
        BlueHeader hdr; std::memset(&hdr, 0, sizeof(hdr));
        std::strncpy(hdr.version, "BLUE", 4); std::strncpy(hdr.head_rep, "EEEI", 4); std::strncpy(hdr.data_rep, "EEEI", 4);
        hdr.type = 1000; hdr.format[0] = 'C'; hdr.format[1] = 'F'; 
        hdr.timecode = start_time; hdr.data_start = 512.0; hdr.data_size = raw_buffer.size();
        hdr.xstart = 0.0; hdr.xdelta = 1.0; hdr.xunits = 1;
        
        std::string buffer;
        buffer.reserve(sizeof(BlueHeader) + raw_buffer.size());
        buffer.append(reinterpret_cast<const char*>(&hdr), sizeof(BlueHeader));
        buffer.append(reinterpret_cast<const char*>(raw_buffer.data()), raw_buffer.size());
        
        return py::bytes(buffer);
    }, py::arg("input_file"), py::arg("start_time"), py::arg("duration"), py::arg("max_points"), py::call_guard<py::gil_scoped_release>());
    
    m.def("run_fft_pipeline", [](const std::string& input_file, 
                                 double center_freq, double zoom_center, double zoom_bw, 
                                 double start_time, double duration, size_t window_size, int smoothing) {
        DspEngine engine(window_size);
        DspEngine::StreamConfig config;
        config.filename = input_file;
        int channels; double sample_rate; bool is_wav, is_blue; std::string format_str; double timecode;
        double dummy_cf;
        engine.get_file_info(input_file, channels, sample_rate, is_wav, is_blue, format_str, timecode, dummy_cf);
        config.is_wav = is_wav; config.is_blue = is_blue; config.sample_rate = sample_rate;
        config.center_freq = center_freq; config.zoom_center = zoom_center; config.zoom_bw = zoom_bw;
        config.window_size = window_size; config.time_smoothing = smoothing;
        double actual_start_time = (start_time == 0.0 && timecode > 0.0) ? timecode : start_time;
        config.start_time = actual_start_time; config.end_time = duration > 0 ? actual_start_time + duration : 0;
        
        auto result = engine.process_file_streaming(config);
        
        const std::vector<double>& out_data = result.avg_fft;
        BlueHeader hdr; std::memset(&hdr, 0, sizeof(hdr));
        std::strncpy(hdr.version, "BLUE", 4); std::strncpy(hdr.head_rep, "EEEI", 4); std::strncpy(hdr.data_rep, "EEEI", 4);
        hdr.type = 1000; hdr.format[0] = 'S'; hdr.format[1] = 'F'; 
        hdr.timecode = result.original_start_time; hdr.data_start = 512.0; hdr.data_size = out_data.size() * sizeof(float);
        hdr.xstart = result.actual_zoom_center - (result.actual_zoom_bw / 2.0); hdr.xdelta = result.actual_zoom_bw / out_data.size(); hdr.xunits = 2;
        
        std::string buffer;
        buffer.reserve(sizeof(BlueHeader) + out_data.size() * sizeof(float));
        buffer.append(reinterpret_cast<const char*>(&hdr), sizeof(BlueHeader));
        
        std::vector<float> out_f(out_data.size());
        
        double cmin = 1e9;
        double cmax = -1e9;
        size_t trim = out_data.size() * 0.1;
        for (size_t i = 0; i < out_data.size(); ++i) {
            out_f[i] = static_cast<float>(out_data[i]);
            if (i >= trim && i < out_data.size() - trim) {
                if (out_data[i] < cmin) cmin = out_data[i];
                if (out_data[i] > cmax) cmax = out_data[i];
            }
        }
        
        // Cap dynamic range to 85 dB
        if (cmax != -1e9) {
            cmin = std::max(cmin, cmax - 85.0);
        }
        
        buffer.append(reinterpret_cast<const char*>(out_f.data()), out_f.size() * sizeof(float));
        
        py::gil_scoped_acquire acquire;
        return py::make_tuple(py::bytes(buffer), cmin, cmax);
    }, py::arg("input_file"), py::arg("center_freq"), py::arg("zoom_center"), py::arg("zoom_bw"), py::arg("start_time"), py::arg("duration"), py::arg("window_size"), py::arg("smoothing"), py::call_guard<py::gil_scoped_release>());

    m.def("run_psd_pipeline", [](const std::string& input_file, 
                                 double center_freq, double zoom_center, double zoom_bw, 
                                 double start_time, double duration, size_t window_size, int smoothing) {
        DspEngine engine(window_size);
        DspEngine::StreamConfig config;
        config.filename = input_file;
        int channels; double sample_rate; bool is_wav, is_blue; std::string format_str; double timecode;
        double dummy_cf;
        engine.get_file_info(input_file, channels, sample_rate, is_wav, is_blue, format_str, timecode, dummy_cf);
        config.is_wav = is_wav; config.is_blue = is_blue; config.sample_rate = sample_rate;
        config.center_freq = center_freq; config.zoom_center = zoom_center; config.zoom_bw = zoom_bw;
        config.window_size = window_size; config.time_smoothing = smoothing; config.step_size = 0;
        double actual_start_time = (start_time == 0.0 && timecode > 0.0) ? timecode : start_time;
        config.start_time = actual_start_time; config.end_time = duration > 0 ? actual_start_time + duration : 0;
        
        auto result = engine.process_file_streaming(config);
        
        if (result.spectrogram.empty()) throw std::runtime_error("No PSD data generated");
        size_t frames = result.spectrogram.size(); size_t frame_size = result.spectrogram[0].size();
        size_t total_elements = frames * frame_size;
        BlueHeader hdr; std::memset(&hdr, 0, sizeof(hdr));
        std::strncpy(hdr.version, "BLUE", 4); std::strncpy(hdr.head_rep, "EEEI", 4); std::strncpy(hdr.data_rep, "EEEI", 4);
        hdr.type = 2000; hdr.format[0] = 'S'; hdr.format[1] = 'F'; 
        hdr.timecode = result.original_start_time; hdr.data_start = 512.0; hdr.data_size = total_elements * sizeof(float);
        hdr.xstart = result.actual_zoom_center - (result.actual_zoom_bw / 2.0); hdr.xdelta = result.actual_zoom_bw / frame_size; hdr.xunits = 2;
        hdr.ystart = result.original_start_time; hdr.ydelta = static_cast<double>(result.actual_step_size) / config.sample_rate; hdr.yunits = 1;
        hdr.subsize = static_cast<int32_t>(frame_size);
        
        std::string buffer;
        buffer.reserve(sizeof(BlueHeader) + total_elements * sizeof(float));
        buffer.append(reinterpret_cast<const char*>(&hdr), sizeof(BlueHeader));
        
        double cmin = 1e9;
        double cmax = -1e9;
        
        const std::vector<double>& avg_data = result.avg_fft;
        if (!avg_data.empty()) {
            size_t avg_trim = avg_data.size() * 0.1;
            for (size_t i = 0; i < avg_data.size(); ++i) {
                if (i >= avg_trim && i < avg_data.size() - avg_trim) {
                    if (avg_data[i] < cmin) cmin = avg_data[i];
                    if (avg_data[i] > cmax) cmax = avg_data[i];
                }
            }
        }
        
        for (const auto& row : result.spectrogram) {
            std::vector<float> row_f(row.size());
            for (size_t i = 0; i < row.size(); ++i) {
                row_f[i] = static_cast<float>(row[i]);
            }
            buffer.append(reinterpret_cast<const char*>(row_f.data()), row_f.size() * sizeof(float));
        }
        
        if (cmax != -1e9) {
            cmin = std::max(cmin, cmax - 85.0);
        }
        
        py::gil_scoped_acquire acquire;
        return py::make_tuple(py::bytes(buffer), cmin, cmax);
    }, py::arg("input_file"), py::arg("center_freq"), py::arg("zoom_center"), py::arg("zoom_bw"), py::arg("start_time"), py::arg("duration"), py::arg("window_size"), py::arg("smoothing"), py::call_guard<py::gil_scoped_release>());

    m.def("run_plot_pipeline", [](const std::string& input_file, const std::string& out_format,
                                 double center_freq, double zoom_center, double zoom_bw, 
                                 double start_time, double duration, size_t window_size, int smoothing,
                                 bool plot_fft, bool plot_waterfall, bool plot_time_domain, bool plot_constellation, const std::string& colormap, int width, int height,
                                 const std::string& theme, const std::string& fill_mode, const std::string& fill_color_hex,
                                 double zmin, double zmax) {
        DspEngine engine(width);
        DspEngine::StreamConfig config;
        config.filename = input_file;
        int channels; double sample_rate; bool is_wav, is_blue; std::string format_str; double timecode;
        double dummy_cf;
        engine.get_file_info(input_file, channels, sample_rate, is_wav, is_blue, format_str, timecode, dummy_cf);
        config.is_wav = is_wav; config.is_blue = is_blue; config.sample_rate = sample_rate;
        config.center_freq = center_freq; config.zoom_center = zoom_center; config.zoom_bw = zoom_bw;
        config.window_size = window_size; config.time_smoothing = smoothing; config.step_size = 0;
        double actual_start_time = (start_time == 0.0 && timecode > 0.0) ? timecode : start_time;
        config.start_time = actual_start_time; config.end_time = duration > 0 ? actual_start_time + duration : 0;
        config.output_width = width; config.output_height = height;
        
        auto result = engine.process_file_streaming(config);
        
        double cmin = 1e9;
        double cmax = -1e9;
        const std::vector<double>& avg_data = result.avg_fft;
        if (!avg_data.empty()) {
            size_t avg_trim = avg_data.size() * 0.1;
            for (size_t i = 0; i < avg_data.size(); ++i) {
                if (i >= avg_trim && i < avg_data.size() - avg_trim) {
                    if (avg_data[i] < cmin) cmin = avg_data[i];
                    if (avg_data[i] > cmax) cmax = avg_data[i];
                }
            }
        }
        if (cmax != -1e9) {
            cmin = std::max(cmin, cmax - 85.0);
        } else {
            cmin = -100.0; cmax = -20.0;
        }

        double final_min_db = (zmin > -999.0) ? zmin : cmin;
        double final_max_db = (zmax < 999.0) ? zmax : cmax;

        double total_duration_sec = (config.sample_rate > 0.0) ? result.spectrogram.size() * result.actual_step_size / config.sample_rate : 0.0;
        double z_center = result.actual_zoom_center;
        double fs = result.actual_zoom_bw;
        
        std::vector<double> freq_bins;
        for (size_t i = 0; i < (size_t)width; ++i) {
            freq_bins.push_back((z_center - fs/2.0) + (i * fs / width));
        }

        std::string start_time_str = format_timecode(actual_start_time);
        
        std::vector<uint8_t> out_buffer;
        
        if (plot_waterfall && !result.spectrogram.empty()) {
            PlotGenerator::generate_fast_waterfall_mem(result.spectrogram, out_buffer, width, height, colormap,
                final_min_db, final_max_db, z_center, fs, start_time_str, total_duration_sec, true, true, out_format, 10, 10, "", 90, 8, "", -1.0, 0.0, 0.0, 0.0, "red", theme);
        } else if (plot_fft && !result.spectrogram.empty()) {
            size_t num_frames = result.spectrogram.size();
            size_t frame_size = result.spectrogram[0].size();
            std::vector<double> avg_mag(frame_size, 0.0);
            for (size_t r = 0; r < num_frames; ++r) {
                for (size_t i = 0; i < frame_size; ++i) {
                    avg_mag[i] += result.spectrogram[r][i];
                }
            }
            for (size_t i = 0; i < frame_size; ++i) {
                avg_mag[i] /= num_frames;
            }
            PlotGenerator::generate_fast_fft_plot_mem(freq_bins, avg_mag, out_buffer, width, height, final_min_db, final_max_db, z_center, fs, true, true, out_format, 10, 15, "", 90, 8, colormap, "", theme, fill_mode, fill_color_hex);
        } else if (plot_time_domain) {
            size_t target_pts = 10000;
            std::vector<uint8_t> td_buf = DspTimeDomain::generate_time_domain_envelope(input_file, start_time, duration, target_pts);
            
            BlueHeader hdr;
            std::memcpy(&hdr, td_buf.data(), sizeof(BlueHeader));
            size_t data_offset = static_cast<size_t>(hdr.data_start);
            size_t actual_floats = hdr.data_size / sizeof(float);
            
            std::vector<float> time_domain_data(actual_floats);
            const float* in_ptr = reinterpret_cast<const float*>(td_buf.data() + data_offset);
            
            double actual_max_val = -1000000.0;
            double actual_min_val = 1000000.0;
            for (size_t i = 0; i < actual_floats; ++i) {
                time_domain_data[i] = in_ptr[i];
                if (time_domain_data[i] > actual_max_val) actual_max_val = time_domain_data[i];
                if (time_domain_data[i] < actual_min_val) actual_min_val = time_domain_data[i];
            }
            
            double final_min_val = (zmin > -999.0) ? zmin : actual_min_val;
            double final_max_val = (zmax < 999.0) ? zmax : actual_max_val;
            double act_start_time = start_time;
            
            bool is_cpx = (hdr.format[0] == 'C');
            size_t floats_per_bucket = is_cpx ? 4 : 2;
            size_t actual_buckets = actual_floats / floats_per_bucket;
            
            PlotGenerator::generate_time_domain_plot_mem(time_domain_data, is_cpx, out_buffer, width, height, final_min_val, final_max_val, act_start_time, actual_buckets * (hdr.xdelta * 2.0), true, true, out_format, 10, 15, "", 90, 8, "", theme, fill_color_hex, "#0088FF");
        } else if (plot_constellation) {
            size_t max_points = 100000;
            std::vector<uint8_t> raw_buf = DspTimeDomain::extract_raw_iq(input_file, start_time, duration, max_points);
            if (!raw_buf.empty()) {
                size_t num_floats = raw_buf.size() / sizeof(float);
                std::vector<float> iq_data(num_floats);
                std::memcpy(iq_data.data(), raw_buf.data(), raw_buf.size());
                
                double actual_max_val = -1000000.0;
                double actual_min_val = 1000000.0;
                for (size_t i = 0; i < num_floats; ++i) {
                    if (iq_data[i] > actual_max_val) actual_max_val = iq_data[i];
                    if (iq_data[i] < actual_min_val) actual_min_val = iq_data[i];
                }
                double final_min_val = (zmin > -999.0) ? zmin : actual_min_val;
                double final_max_val = (zmax < 999.0) ? zmax : actual_max_val;
                
                PlotGenerator::generate_constellation_plot_mem(iq_data, out_buffer, width, height, final_min_val, final_max_val, true, true, out_format, 10, 10, "", 90, 8, "", theme, fill_color_hex);
            }
        }

        
        std::string s_buf(out_buffer.begin(), out_buffer.end());
        return py::bytes(s_buf);
    }, py::arg("input_file"), py::arg("out_format"), py::arg("center_freq"), py::arg("zoom_center"), py::arg("zoom_bw"), py::arg("start_time"), py::arg("duration"), py::arg("window_size"), py::arg("smoothing"), py::arg("plot_fft"), py::arg("plot_waterfall"), py::arg("plot_time_domain"), py::arg("plot_constellation"), py::arg("colormap"), py::arg("width"), py::arg("height"), py::arg("theme") = "dark", py::arg("fill_mode") = "gradient", py::arg("fill_color") = "#00FF00", py::arg("zmin") = -1000.0, py::arg("zmax") = 1000.0, py::call_guard<py::gil_scoped_release>());
}
