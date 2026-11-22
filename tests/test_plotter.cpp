#include <catch2/catch_all.hpp>
#include "../plot_generator.hpp"
#include <fstream>
#include <cstdio>
#include <cstring>

namespace {

std::vector<uint8_t> read_binary_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    REQUIRE(f.is_open());
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (size > 0) {
        f.read(reinterpret_cast<char*>(buf.data()), size);
    }
    return buf;
}

bool is_valid_png(const std::vector<uint8_t>& buf) {
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    return buf.size() > 8 && std::memcmp(buf.data(), sig, 8) == 0;
}

bool is_valid_jpg(const std::vector<uint8_t>& buf) {
    return buf.size() > 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF;
}

bool is_valid_webp(const std::vector<uint8_t>& buf) {
    return buf.size() > 12 &&
           std::memcmp(buf.data(), "RIFF", 4) == 0 &&
           std::memcmp(buf.data() + 8, "WEBP", 4) == 0;
}

bool is_valid_avif(const std::vector<uint8_t>& buf) {
    return buf.size() > 12 &&
           std::memcmp(buf.data() + 4, "ftyp", 4) == 0 &&
           (std::memcmp(buf.data() + 8, "avif", 4) == 0 || std::memcmp(buf.data() + 8, "avis", 4) == 0);
}

} // namespace

TEST_CASE("PlotGenerator Formats and Normalized Image Quality", "[plotter]") {
    int width = 128;
    int height = 128;

    std::vector<std::vector<double>> spectrogram(32, std::vector<double>(64, -80.0));
    for (size_t r = 0; r < spectrogram.size(); ++r) {
        for (size_t c = 0; c < spectrogram[r].size(); ++c) {
            spectrogram[r][c] = -90.0 + static_cast<double>((r * 7 + c * 13) % 70);
        }
    }

    std::vector<double> freq_bins(64);
    std::vector<double> mag_db(64);
    for (size_t i = 0; i < 64; ++i) {
        freq_bins[i] = 100.0 + static_cast<double>(i) * 0.05;
        mag_db[i] = -85.0 + static_cast<double>((i * 19) % 60);
    }

    SECTION("Memory and file output for PNG, JPG, WEBP, AVIF") {
        struct FormatCheck {
            std::string fmt;
            bool (*validator)(const std::vector<uint8_t>&);
        };
        std::vector<FormatCheck> formats = {
            {"png", is_valid_png},
            {"jpg", is_valid_jpg},
            {"jpeg", is_valid_jpg},
            {"webp", is_valid_webp},
            {"avif", is_valid_avif}
        };

        for (const auto& fc : formats) {
            std::vector<uint8_t> wf_mem;
            PlotGenerator::generate_fast_waterfall_mem(
                spectrogram, wf_mem, width, height, "turbo",
                -100.0, -20.0, 100.0, 2.0, "2026-01-01T00:00:00", 1.0,
                true, true, fc.fmt, 4, 4, "Waterfall Test", 80
            );
            REQUIRE(fc.validator(wf_mem));

            std::vector<uint8_t> fft_mem;
            PlotGenerator::generate_fast_fft_plot_mem(
                freq_bins, mag_db, fft_mem, width, height,
                -100.0, -20.0, 100.0, 2.0,
                true, true, fc.fmt, 4, 4, "FFT Test", 80
            );
            REQUIRE(fc.validator(fft_mem));

            std::string wf_file = "/tmp/test_waterfall_out." + fc.fmt;
            PlotGenerator::generate_fast_waterfall(
                spectrogram, wf_file, width, height, "gqrx",
                -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
                true, true, fc.fmt, 4, 4, "WF File", 80
            );
            auto wf_disk = read_binary_file(wf_file);
            REQUIRE(fc.validator(wf_disk));
            std::remove(wf_file.c_str());

            std::string fft_file = "/tmp/test_fft_out." + fc.fmt;
            PlotGenerator::generate_fast_fft_plot(
                freq_bins, mag_db, fft_file, width, height,
                -100.0, -20.0, 100.0, 2.0,
                true, true, fc.fmt, 4, 4, "FFT File", 80
            );
            auto fft_disk = read_binary_file(fft_file);
            REQUIRE(fc.validator(fft_disk));
            std::remove(fft_file.c_str());
        }
    }

    SECTION("Time domain and constellation plots support WEBP and AVIF") {
        std::vector<float> td_data(128 * 4);
        for (size_t i = 0; i < 128; ++i) {
            td_data[i * 4 + 0] = -0.5f;
            td_data[i * 4 + 1] = -0.4f;
            td_data[i * 4 + 2] = 0.5f;
            td_data[i * 4 + 3] = 0.4f;
        }
        std::vector<float> iq_data(256 * 2);
        for (size_t i = 0; i < 256; ++i) {
            iq_data[i * 2 + 0] = (i % 2 == 0) ? 0.7f : -0.7f;
            iq_data[i * 2 + 1] = (i % 4 < 2) ? 0.7f : -0.7f;
        }

        for (const std::string& fmt : {"png", "jpg", "webp", "avif"}) {
            std::vector<uint8_t> td_buf;
            PlotGenerator::generate_time_domain_plot_mem(
                td_data, true, td_buf, width, height, -1.0, 1.0, 0.0, 1.0,
                true, true, fmt, 4, 4, "TD", 75
            );
            REQUIRE(!td_buf.empty());

            std::vector<uint8_t> const_buf;
            PlotGenerator::generate_constellation_plot_mem(
                iq_data, const_buf, width, height, -1.0, 1.0,
                true, true, fmt, 4, 4, "Constellation", 75
            );
            REQUIRE(!const_buf.empty());

            if (fmt == "webp") {
                REQUIRE(is_valid_webp(td_buf));
                REQUIRE(is_valid_webp(const_buf));
            } else if (fmt == "avif") {
                REQUIRE(is_valid_avif(td_buf));
                REQUIRE(is_valid_avif(const_buf));
            }
        }
    }

    SECTION("Image quality 1-100 affects compression across formats") {
        for (const std::string& fmt : {"jpg", "webp", "avif", "png"}) {
            std::vector<uint8_t> low_q;
            std::vector<uint8_t> high_q;
            PlotGenerator::generate_fast_waterfall_mem(
                spectrogram, low_q, width, height, "turbo",
                -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
                true, true, fmt, 4, 4, "Quality Test", 10
            );
            PlotGenerator::generate_fast_waterfall_mem(
                spectrogram, high_q, width, height, "turbo",
                -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
                true, true, fmt, 4, 4, "Quality Test", 95
            );
            REQUIRE(!low_q.empty());
            REQUIRE(!high_q.empty());
            if (fmt != "png") {
                REQUIRE(low_q.size() < high_q.size());
            } else {
                REQUIRE(is_valid_png(low_q));
                REQUIRE(is_valid_png(high_q));
            }
        }
    }

    SECTION("Font size, box alpha, theme, and FFT fill options") {
        std::vector<uint8_t> wf_alpha0;
        std::vector<uint8_t> wf_alpha1;
        PlotGenerator::generate_fast_waterfall_mem(
            spectrogram, wf_alpha0, width, height, "gqrx",
            -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
            true, true, "png", 4, 4, "Box Alpha 0", 50, -1, "",
            0.2, 0.4, 100.0, 0.8, "#00FF00", "dark", 0.0f, 16
        );
        PlotGenerator::generate_fast_waterfall_mem(
            spectrogram, wf_alpha1, width, height, "gqrx",
            -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
            true, true, "png", 4, 4, "Box Alpha 1", 50, -1, "",
            0.2, 0.4, 100.0, 0.8, "#00FF00", "dark", 0.9f, 16
        );
        REQUIRE(is_valid_png(wf_alpha0));
        REQUIRE(is_valid_png(wf_alpha1));
        REQUIRE(wf_alpha0 != wf_alpha1);

        std::vector<uint8_t> wf_no_peaks;
        std::vector<uint8_t> wf_peaks_cell;
        std::vector<uint8_t> wf_peaks_outline;
        // Adjacent/overlapping cells (5, 10) and (6, 10)
        std::vector<std::pair<int, int>> peaks = {{5, 10}, {6, 10}, {15, 30}};
        PlotGenerator::generate_fast_waterfall_mem(
            spectrogram, wf_no_peaks, width, height, "gqrx",
            -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
            true, true, "png", 4, 4, "Peaks Test", 50, -1, "",
            -1.0, 0.0, 0.0, 0.0, "#00FF00", "dark", 0.3f, 12
        );
        PlotGenerator::generate_fast_waterfall_mem(
            spectrogram, wf_peaks_cell, width, height, "gqrx",
            -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
            true, true, "png", 4, 4, "Peaks Test", 50, -1, "",
            -1.0, 0.0, 0.0, 0.0, "#00FF00", "dark", 0.3f, 12, peaks, "cell"
        );
        PlotGenerator::generate_fast_waterfall_mem(
            spectrogram, wf_peaks_outline, width, height, "gqrx",
            -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
            true, true, "png", 4, 4, "Peaks Test", 50, -1, "",
            -1.0, 0.0, 0.0, 0.0, "#00FF00", "dark", 0.3f, 12, peaks, "outline"
        );
        REQUIRE(is_valid_png(wf_no_peaks));
        REQUIRE(is_valid_png(wf_peaks_cell));
        REQUIRE(is_valid_png(wf_peaks_outline));
        REQUIRE(wf_peaks_cell != wf_no_peaks);
        REQUIRE(wf_peaks_outline != wf_no_peaks);
        REQUIRE(wf_peaks_outline != wf_peaks_cell);

        std::vector<uint8_t> wf_peaks_thick;
        PlotGenerator::generate_fast_waterfall_mem(
            spectrogram, wf_peaks_thick, width, height, "gqrx",
            -100.0, -20.0, 100.0, 2.0, "0.00s", 1.0,
            true, true, "png", 4, 4, "Peaks Test", 50, -1, "",
            -1.0, 0.0, 0.0, 0.0, "#00FF00", "dark", 0.3f, 12, peaks, "outline", 3
        );
        REQUIRE(is_valid_png(wf_peaks_thick));
        REQUIRE(wf_peaks_thick != wf_peaks_outline);

        std::string fft_custom = "/tmp/test_fft_custom.webp";
        PlotGenerator::generate_fast_fft_plot(
            freq_bins, mag_db, fft_custom, width, height,
            -100.0, -20.0, 100.0, 2.0,
            true, true, "webp", 4, 4, "Custom FFT", 50, -1, "turbo", "",
            14, "light", "solid", "#FF5500", 0.4f
        );
        auto fft_disk = read_binary_file(fft_custom);
        REQUIRE(is_valid_webp(fft_disk));
        std::remove(fft_custom.c_str());
    }
}
