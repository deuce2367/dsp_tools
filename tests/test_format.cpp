#include <catch2/catch_all.hpp>
#include "test_utils.hpp"
#include "../dsp_format.cpp"

TEST_CASE("DSP Format", "[format]") {
    std::string in_file = "/tmp/test_in_format.prm";
    std::string out_file = "/tmp/test_out_format.prm";
    
    SECTION("Convert Complex to Real Magnitude") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, float>(in_file, out_file, false, ComplexMethod::Pad, 255, true, ExtractMethod::Mag);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 1024); // Now real, so 1024 samples
    }
    
    SECTION("Cast Float to Int16") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        // Output type 'I' means int16
        format_data<float, int16_t>(in_file, out_file, false, ComplexMethod::Pad, 255, false, ExtractMethod::I);
        
        // Output is Int16, we can't easily read it with read_bluefile_data since it expects float
        // But the file should have been written successfully
        BlueHeader hdr = read_bluefile_header(out_file);
        REQUIRE(hdr.format[0] == 'C');
        REQUIRE(hdr.format[1] == 'I');
    }
    SECTION("Extract Phase") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, float>(in_file, out_file, false, ComplexMethod::Pad, 255, true, ExtractMethod::Phase);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 1024);
    }
    
    SECTION("Extract I") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, float>(in_file, out_file, false, ComplexMethod::Pad, 255, true, ExtractMethod::I);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 1024);
    }

    SECTION("Extract Q") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, float>(in_file, out_file, false, ComplexMethod::Pad, 255, true, ExtractMethod::Q);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 1024);
    }

    SECTION("Convert Real to Complex (Hilbert)") {
        // Need a real input file
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        // first convert to real
        std::string tmp_file = "/tmp/test_format_real.prm";
        format_data<float, float>(in_file, tmp_file, false, ComplexMethod::Pad, 255, true, ExtractMethod::I);
        
        // now test real to complex with hilbert
        format_data<float, float>(tmp_file, out_file, true, ComplexMethod::Hilbert, 255, false, ExtractMethod::I);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 1024 * 2);
    }

    SECTION("Convert Real to Complex (Pack)") {
        std::string tmp_file = "/tmp/test_format_real2.prm";
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, float>(in_file, tmp_file, false, ComplexMethod::Pad, 255, true, ExtractMethod::I);
        
        format_data<float, float>(tmp_file, out_file, true, ComplexMethod::Pack, 255, false, ExtractMethod::I);
        
        auto out_data = test_utils::read_bluefile_data(out_file);
        REQUIRE(out_data.size() == 512 * 2); // Pack divides samples by 2
    }

    SECTION("Cast Float to Int8") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, int8_t>(in_file, out_file, false, ComplexMethod::Pad, 255, false, ExtractMethod::I);
        BlueHeader hdr = read_bluefile_header(out_file);
        REQUIRE(hdr.format[1] == 'B');
    }
    
    SECTION("Cast Float to Int32") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, int32_t>(in_file, out_file, false, ComplexMethod::Pad, 255, false, ExtractMethod::I);
        BlueHeader hdr = read_bluefile_header(out_file);
        REQUIRE(hdr.format[1] == 'L');
    }

    SECTION("Cast Float to Double") {
        test_utils::generate_sine_bluefile(in_file, 4000000.0, 1000000.0, 1024);
        format_data<float, double>(in_file, out_file, false, ComplexMethod::Pad, 255, false, ExtractMethod::I);
        BlueHeader hdr = read_bluefile_header(out_file);
        REQUIRE(hdr.format[1] == 'D');
    }

    SECTION("Read non-existent file throws") {
        REQUIRE_THROWS(read_bluefile_header("/tmp/does_not_exist_file.prm"));
    }

    SECTION("Cast Complex Float to Double") {
        test_utils::generate_interference_bluefile(in_file, 4000000.0, 1024);
        format_data<float, double>(in_file, out_file, true, ComplexMethod::Pad, 255, false, ExtractMethod::I);
        BlueHeader hdr = read_bluefile_header(out_file);
        REQUIRE(hdr.format[1] == 'D');
    }
}
