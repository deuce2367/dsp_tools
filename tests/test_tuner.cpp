#include <catch2/catch_all.hpp>
#include "test_utils.hpp"
#include "../dsp_tuner_lib.cpp"

TEST_CASE("DSP Tuner", "[tuner]") {
    std::string in_file = "/tmp/test_in_tuner.prm";
    std::string out_file = "/tmp/test_out_tuner.prm";
    
    // Generate a 1 MHz sine wave at 4 MHz sample rate
    test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 16384);
    
    SECTION("Tuner centers signal and decimates") {
        // Tune to 1 MHz, bandwidth 1 MHz (decimate by 4)
        run_tuner_pipeline(in_file, out_file, 1000000.0, 1000000.0, 0.0, 0.0, 0.0, false, TunerQuality::Normal);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 4096 * 2); // Decimated by 4
        
        // Output should be a DC signal (0 Hz) since we tuned exactly to 1 MHz
        // Check power of output
        double power = 0.0;
        for (float v : out_data) power += v * v;
        REQUIRE(power > 1000.0);
    }

    SECTION("Tuner uses file center") {
        run_tuner_pipeline(in_file, out_file, 0.0, 1000000.0, 0.0, 0.0, 1000000.0, true, TunerQuality::Draft);
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 4096 * 2);
    }
}
