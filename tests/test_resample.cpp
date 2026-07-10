#include <catch2/catch_all.hpp>
#include "test_utils.hpp"
#include "../dsp_resample.cpp"

TEST_CASE("DSP Resample", "[resample]") {
    std::string in_file = "/tmp/test_in_resample.prm";
    std::string out_file = "/tmp/test_out_resample.prm";
    
    test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 16000);
    
    SECTION("Resample to higher rate") {
        resample_data<float>(in_file, out_file, 8000000.0, 1, 1, resample_quality::normal);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        // Interpolation by 2
        REQUIRE(out_data.size() >= 32000 * 2);
    }
    
    SECTION("Resample to lower rate") {
        resample_data<float>(in_file, out_file, 2000000.0, 1, 1, resample_quality::normal);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        // Decimation by 2
        REQUIRE(out_data.size() >= 8000 * 2);
    }
}
