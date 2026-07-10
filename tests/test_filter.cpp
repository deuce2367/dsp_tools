#include <catch2/catch_all.hpp>
#include "test_utils.hpp"
#include "../dsp_filter.cpp"

TEST_CASE("DSP Filter - Lowpass", "[filter]") {
    std::string in_file = "/tmp/test_in_filter.prm";
    std::string out_file = "/tmp/test_out_filter.prm";
    
    // Generate a 1 MHz sine wave at 4 MHz sample rate
    test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 16384);
    
    SECTION("Lowpass filter passes signal") {
        // Cutoff at 1.5 MHz, should pass 1 MHz
        filter_data<float>(in_file, out_file, "lowpass", 1500000.0, 0.0, 255);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2); // Complex float
        
        // Check power of output (should be preserved roughly)
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power > 1000.0);
    }
    
    SECTION("Lowpass filter blocks signal") {
        // Cutoff at 0.5 MHz, should block 1 MHz
        filter_data<float>(in_file, out_file, "lowpass", 500000.0, 0.0, 255);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
        
        // Check power of output (should be near 0)
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power < 100.0);
    }

    SECTION("Highpass filter passes signal") {
        filter_data<float>(in_file, out_file, "highpass", 500000.0, 0.0, 255);
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
        
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power > 1000.0);
    }
    
    SECTION("Highpass filter blocks signal") {
        filter_data<float>(in_file, out_file, "highpass", 1500000.0, 0.0, 255);
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
        
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power < 100.0);
    }

    SECTION("Bandpass filter passes signal") {
        filter_data<float>(in_file, out_file, "bandpass", 800000.0, 1200000.0, 255);
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
        
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power > 1000.0);
    }

    SECTION("Bandstop filter blocks signal") {
        filter_data<float>(in_file, out_file, "bandstop", 800000.0, 1200000.0, 255);
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
        
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power < 100.0);
    }
}
