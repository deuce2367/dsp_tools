#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../bluefile_io.hpp"
#include <cstdio>
#include <vector>
#include <string>

TEST_CASE("Bluefile IO CRUD Operations", "[bluefile]") {
    std::string test_file = "test_crud_bluefile.prm";
    
    // 1. Create a dummy BlueHeader and extended data
    BlueHeader hdr{};
    std::strncpy(hdr.version, "BLUE", 4);
    std::strncpy(hdr.head_rep, "EEEI", 4);
    std::strncpy(hdr.data_rep, "EEEI", 4);
    hdr.type = 1000;
    hdr.format[0] = 'C'; hdr.format[1] = 'I';
    hdr.data_start = 512.0;
    hdr.data_size = 1000.0; // 1000 bytes of data
    
    std::vector<uint8_t> ext_data(256, 0xAB); // 256 bytes of extended header
    
    // 2. Prepare extended header metadata (calculates padding and ext_start)
    prepare_bluefile_ext_header(hdr, ext_data);
    
    // Verify padding math
    // size = 512 + 1000 = 1512
    // padding = (512 - (1512 % 512)) % 512 = (512 - 488) % 512 = 24
    // ext_start = (1512 + 24) / 512 = 1536 / 512 = 3
    REQUIRE(hdr.ext_start == 3);
    REQUIRE(hdr.ext_size == 256);
    
    // 3. Write header
    write_bluefile_header(test_file, hdr);
    
    // Write fake data
    std::vector<uint8_t> fake_data(static_cast<size_t>(hdr.data_size), 0xDD);
    int fd = open(test_file.c_str(), O_WRONLY | O_APPEND);
    write(fd, fake_data.data(), fake_data.size());
    close(fd);
    
    // Write extended header
    write_bluefile_ext_header(test_file, hdr, ext_data);
    
    // 4. Read header and extended header back
    BlueHeader read_hdr = read_bluefile_header(test_file);
    REQUIRE(std::strncmp(read_hdr.version, "BLUE", 4) == 0);
    REQUIRE(read_hdr.ext_start == 3);
    REQUIRE(read_hdr.ext_size == 256);
    
    std::vector<uint8_t> read_ext_data = read_bluefile_ext_header(test_file, read_hdr);
    REQUIRE(read_ext_data.size() == 256);
    REQUIRE(read_ext_data[0] == 0xAB);
    REQUIRE(read_ext_data[255] == 0xAB);
    
    // Clean up
    std::remove(test_file.c_str());
}
