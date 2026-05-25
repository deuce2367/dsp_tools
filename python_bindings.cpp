#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "dsp_engine.hpp"
#include "plot_generator.hpp"

namespace py = pybind11;

PYBIND11_MODULE(dsp_plotter_py, m) {
    m.doc() = "DSP Plotter Python bindings";

    py::class_<DspEngine::StreamConfig>(m, "StreamConfig")
        .def(py::init<>())
        .def_readwrite("filename", &DspEngine::StreamConfig::filename)
        .def_readwrite("data_type", &DspEngine::StreamConfig::data_type)
        .def_readwrite("sample_rate", &DspEngine::StreamConfig::sample_rate)
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
            int channels;
            double sample_rate;
            bool is_wav;
            bool is_blue;
            std::string format_str;
            double timecode;
            self.get_file_info(filename, channels, sample_rate, is_wav, is_blue, format_str, timecode);
            return py::make_tuple(channels, sample_rate, is_wav, is_blue, format_str, timecode);
        });

    py::class_<PlotGenerator>(m, "PlotGenerator")
        .def_static("generate_fast_waterfall", &PlotGenerator::generate_fast_waterfall,
            py::arg("spectrogram_db"), py::arg("output_filename"), py::arg("out_width"), py::arg("out_height"),
            py::arg("colormap"), py::arg("min_db"), py::arg("max_db"), py::arg("center_freq_mhz"), py::arg("bandwidth_mhz"),
            py::arg("start_time_iso"), py::arg("total_duration_sec"), py::arg("draw_grid") = true, py::arg("draw_labels") = true,
            py::arg("out_format") = "png", py::arg("num_x_ticks") = 10, py::arg("num_y_ticks") = 10, py::arg("title") = "",
            py::arg("jpeg_quality") = 90, py::arg("png_compression") = 8)
        .def_static("generate_fast_fft_plot", &PlotGenerator::generate_fast_fft_plot,
            py::arg("frequency_bins"), py::arg("magnitude_db"), py::arg("output_filename"), py::arg("out_width"), py::arg("out_height"),
            py::arg("min_db"), py::arg("max_db"), py::arg("center_freq_mhz"), py::arg("bandwidth_mhz"),
            py::arg("draw_grid") = true, py::arg("draw_labels") = true, py::arg("out_format") = "png",
            py::arg("num_x_ticks") = 10, py::arg("num_y_ticks") = 10, py::arg("title") = "",
            py::arg("jpeg_quality") = 90, py::arg("png_compression") = 8, py::arg("colormap_name") = "jet");
}
