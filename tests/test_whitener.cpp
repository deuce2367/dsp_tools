#include <catch2/catch_all.hpp>
#include "test_utils.hpp"
#include "../dsp_whitener.cpp"

TEST_CASE("DSP Whitener", "[whitener]") {
    std::string in_file = "/tmp/test_in_whitener.prm";
    std::string out_file = "/tmp/test_out_whitener.prm";
    
    // Generate interference (white noise + CW + pulse)
    test_utils::generate_interference_bluefile(in_file, 4000000.0, 16384);
    
    SECTION("Compress mode") {
        process_whitener<float>(in_file, out_file, 1024, 0.95, 10.0, 128, 0.0, 1.0, "compress", -100.0);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
    }
    
    SECTION("Griffiths mode") {
        process_whitener<float>(in_file, out_file, 1024, 0.95, 10.0, 128, 0.0, 1.0, "griffiths", 10.0);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 16384 * 2);
    }
}
