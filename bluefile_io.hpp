#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#pragma pack(push, 1)
struct BlueHeader {
    char version[4];
    char head_rep[4];
    char data_rep[4];
    int32_t detached;
    int32_t protected_flag;
    int32_t pipe;
    int32_t ext_start;
    int32_t ext_size;
    double data_start;
    double data_size;
    int32_t type;
    char format[2];
    int16_t flagmask;
    double timecode;
    int16_t inlet;
    int16_t outlets;
    int32_t outmask;
    int32_t pipeloc;
    int32_t pipesize;
    double in_byte;
    double out_byte;
    double outbytes[8];
    int32_t keylength;
    char keywords[92];
    
    // Adjunct (Offset 256)
    double xstart;
    double xdelta;
    int32_t xunits;
    int32_t subsize;
    double ystart;
    double ydelta;
    int32_t yunits;
    char padding[212];

};
static_assert(sizeof(BlueHeader) == 512, "BlueHeader must be exactly 512 bytes");
#pragma pack(pop)

struct MmapHandle {
    int fd = -1;
    uint8_t* ptr = nullptr;
    size_t size = 0;
    
    MmapHandle(const std::string& filename) {
        fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1) throw std::runtime_error("Could not open file for mmap: " + filename);
        
        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            throw std::runtime_error("Could not stat file: " + filename);
        }
        size = sb.st_size;
        
        ptr = static_cast<uint8_t*>(mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (ptr == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("mmap failed for file: " + filename);
        }
    }
    
    ~MmapHandle() {
        if (ptr && ptr != MAP_FAILED) munmap(ptr, size);
        if (fd != -1) close(fd);
    }
};

inline void write_bluefile_header(const std::string& filename, const BlueHeader& hdr) {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) throw std::runtime_error("Cannot create output file: " + filename);
    if (write(fd, &hdr, sizeof(BlueHeader)) != sizeof(BlueHeader)) {
        close(fd);
        throw std::runtime_error("Failed to write bluefile header.");
    }
    close(fd);
}

inline BlueHeader read_bluefile_header(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("Cannot open BLUE file: " + filename);
    BlueHeader hdr;
    if (read(fd, &hdr, sizeof(BlueHeader)) != sizeof(BlueHeader)) {
        close(fd);
        throw std::runtime_error("Invalid BLUE file size");
    }
    close(fd);
    if (strncmp(hdr.version, "BLUE", 4) != 0) {
        throw std::runtime_error("Not a valid BLUE file");
    }
    if (hdr.type != 1000 && hdr.type != 2000) {
        throw std::runtime_error("Only Type 1000 and 2000 BLUE files are supported");
    }
    return hdr;
}

inline void update_bluefile_header(const std::string& filename, double timecode, double center_freq) {
    int fd = open(filename.c_str(), O_RDWR);
    if (fd < 0) throw std::runtime_error("Cannot open BLUE file to modify: " + filename);
    BlueHeader hdr;
    if (read(fd, &hdr, sizeof(BlueHeader)) != sizeof(BlueHeader)) {
        close(fd);
        throw std::runtime_error("Invalid BLUE file size");
    }
    
    // Update timecode
    hdr.timecode = timecode;
    
    // Update center_freq in keywords if non-zero or just replace it
    std::string keyword_str(hdr.keywords, strnlen(hdr.keywords, sizeof(hdr.keywords)));
    size_t rf_pos = keyword_str.find("RF_FREQUENCY_MHZ=");
    if (rf_pos != std::string::npos) {
        size_t val_start = rf_pos + 17;
        size_t val_end = keyword_str.find_first_of(";\n ", val_start);
        if (val_end == std::string::npos) val_end = keyword_str.length();
        
        // Remove old RF_FREQUENCY_MHZ
        keyword_str.erase(rf_pos, val_end - rf_pos);
    }
    
    if (center_freq > 0.0) { 
        if (!keyword_str.empty() && keyword_str.back() != '\n') keyword_str += "\n";
        keyword_str += "RF_FREQUENCY_MHZ=" + std::to_string(center_freq) + "\n";
    }
    
    if (keyword_str.length() >= sizeof(hdr.keywords)) {
        keyword_str = keyword_str.substr(0, sizeof(hdr.keywords) - 1);
    }
    std::memset(hdr.keywords, 0, sizeof(hdr.keywords));
    std::strncpy(hdr.keywords, keyword_str.c_str(), sizeof(hdr.keywords) - 1);
    
    lseek(fd, 0, SEEK_SET);
    if (write(fd, &hdr, sizeof(BlueHeader)) != sizeof(BlueHeader)) {
        close(fd);
        throw std::runtime_error("Failed to write modified bluefile header.");
    }
    close(fd);
}

inline std::vector<uint8_t> read_bluefile_ext_header(const std::string& filename, const BlueHeader& hdr) {
    if (hdr.ext_size <= 0 || hdr.ext_start <= 0) return {};
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("Cannot open BLUE file to read ext header");
    lseek(fd, hdr.ext_start, SEEK_SET);
    std::vector<uint8_t> ext_data(hdr.ext_size);
    if (read(fd, ext_data.data(), hdr.ext_size) != hdr.ext_size) {
        close(fd);
        throw std::runtime_error("Failed to read complete extended header");
    }
    close(fd);
    return ext_data;
}

inline void write_bluefile_ext_header(const std::string& filename, const std::vector<uint8_t>& ext_data) {
    if (ext_data.empty()) return;
    int fd = open(filename.c_str(), O_WRONLY | O_APPEND);
    if (fd < 0) throw std::runtime_error("Cannot open output file to append ext header");
    if (write(fd, ext_data.data(), ext_data.size()) != static_cast<ssize_t>(ext_data.size())) {
        close(fd);
        throw std::runtime_error("Failed to write extended header");
    }
    close(fd);
}
