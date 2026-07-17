#include "dsp_convert.hpp"
#include "bluefile_io.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cstring>

#pragma pack(push, 1)
struct RiffHeader {
    char chunkId[4]; // "RIFF"
    uint32_t chunkSize;
    char format[4]; // "WAVE"
};

struct FmtChunk {
    char subchunk1Id[4]; // "fmt "
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
};

struct ChunkHeader {
    char id[4];
    uint32_t size;
};
#pragma pack(pop)

static void generate_sigmf(const std::string& meta_filename, const std::string& data_filename, 
                           const std::string& fmt_str, double sample_rate, size_t offset) {
    std::string sigmf_type = "unknown";
    if (fmt_str == "CF") sigmf_type = "cf32_le";
    else if (fmt_str == "SF") sigmf_type = "rf32_le";
    else if (fmt_str == "CI") sigmf_type = "ci16_le";
    else if (fmt_str == "SI") sigmf_type = "ri16_le";
    else if (fmt_str == "CB") sigmf_type = "ci8";
    else if (fmt_str == "SB") sigmf_type = "ri8";
    else if (fmt_str == "CD") sigmf_type = "cf64_le";
    else if (fmt_str == "SD") sigmf_type = "rf64_le";

    std::ofstream out(meta_filename);
    if (!out) throw std::runtime_error("Could not create SigMF meta file: " + meta_filename);
    
    out << "{\n";
    out << "  \"global\": {\n";
    out << "    \"core:datatype\": \"" << sigmf_type << "\",\n";
    out << "    \"core:sample_rate\": " << sample_rate << ",\n";
    out << "    \"core:version\": \"1.0.0\"\n";
    out << "  },\n";
    out << "  \"captures\": [\n";
    out << "    {\n";
    out << "      \"core:sample_start\": 0\n";
    out << "    }\n";
    out << "  ],\n";
    out << "  \"annotations\": []\n";
    out << "}\n";
    out.close();
}

void run_convert_pipeline(const std::string& input_file, const std::string& output_file, 
                          const std::string& format, double rate, double freq_mhz, bool sigmf, double timecode) {
    std::ifstream in(input_file, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Cannot open input file: " + input_file);
    
    size_t file_size = in.tellg();
    in.seekg(0, std::ios::beg);
    
    size_t data_offset = 0;
    size_t data_size = file_size;
    
    std::string final_format = format;
    double final_rate = rate > 0.0 ? rate : 1.0;
    
    std::string ext = "";
    size_t dot_pos = input_file.find_last_of('.');
    if (dot_pos != std::string::npos) {
        ext = input_file.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    
    if (ext == ".wav") {
        spdlog::info("Parsing WAV header...");
        RiffHeader riff;
        in.read(reinterpret_cast<char*>(&riff), sizeof(riff));
        if (std::strncmp(riff.chunkId, "RIFF", 4) != 0 || std::strncmp(riff.format, "WAVE", 4) != 0) {
            throw std::runtime_error("Not a valid WAV file (missing RIFF/WAVE)");
        }
        
        FmtChunk fmt_chunk;
        bool found_fmt = false;
        ChunkHeader chdr;
        
        while (in.read(reinterpret_cast<char*>(&chdr), sizeof(chdr))) {
            if (std::strncmp(chdr.id, "fmt ", 4) == 0) {
                in.read(reinterpret_cast<char*>(&fmt_chunk.audioFormat), sizeof(FmtChunk) - 8);
                if (chdr.size > sizeof(FmtChunk) - 8) {
                    in.seekg(chdr.size - (sizeof(FmtChunk) - 8), std::ios::cur);
                }
                found_fmt = true;
            } else if (std::strncmp(chdr.id, "data", 4) == 0) {
                data_offset = in.tellg();
                data_size = chdr.size;
                break;
            } else {
                in.seekg(chdr.size, std::ios::cur);
            }
        }
        
        if (!found_fmt || data_offset == 0) {
            throw std::runtime_error("Invalid WAV file structure");
        }
        
        final_rate = fmt_chunk.sampleRate;
        char type_char = (fmt_chunk.numChannels == 2) ? 'C' : 'S';
        char class_char = 'B';
        if (fmt_chunk.audioFormat == 1) {
            if (fmt_chunk.bitsPerSample == 8) class_char = 'B';
            else if (fmt_chunk.bitsPerSample == 16) class_char = 'I';
            else if (fmt_chunk.bitsPerSample == 32) class_char = 'L';
        } else if (fmt_chunk.audioFormat == 3) {
            if (fmt_chunk.bitsPerSample == 32) class_char = 'F';
            else if (fmt_chunk.bitsPerSample == 64) class_char = 'D';
        } else {
            throw std::runtime_error("Unsupported WAV audio format");
        }
        
        final_format = std::string(1, type_char) + class_char;
        spdlog::info("WAV File Detected: {} channels, {} Hz, {} bits, Derived format: {}", 
                     fmt_chunk.numChannels, final_rate, fmt_chunk.bitsPerSample, final_format);
    } else {
        if (final_format.empty()) {
            throw std::runtime_error("RAW files require format parameter (e.g. CF, SI, etc) to be specified.");
        }
        std::transform(final_format.begin(), final_format.end(), final_format.begin(), ::toupper);
        spdlog::info("RAW File processing: Format: {}, Rate: {} Hz", final_format, final_rate);
    }
    
    BlueHeader hdr;
    std::memset(&hdr, 0, sizeof(BlueHeader));
    std::strncpy(hdr.version, "BLUE", 4);
    std::strncpy(hdr.head_rep, "IEEE", 4);
    std::strncpy(hdr.data_rep, "IEEE", 4);
    hdr.type = 1000;
    
    if (final_format.length() >= 2) {
        hdr.format[0] = final_format[0];
        hdr.format[1] = final_format[1];
    }
    
    hdr.data_start = 512.0;
    hdr.data_size = static_cast<double>(data_size);
    hdr.xstart = 0.0;
    hdr.xdelta = 1.0 / final_rate;
    hdr.xunits = 1;
    hdr.ystart = 0.0;
    hdr.ydelta = 1.0;
    hdr.timecode = timecode;
    
    if (freq_mhz > 0.0) {
        std::string keyword_str = "RF_FREQUENCY_MHZ=" + std::to_string(freq_mhz);
        if (keyword_str.length() < sizeof(hdr.keywords)) {
            std::strncpy(hdr.keywords, keyword_str.c_str(), sizeof(hdr.keywords));
        }
    }
    
    write_bluefile_header(output_file, hdr);
    
    int out_fd = open(output_file.c_str(), O_WRONLY | O_APPEND);
    if (out_fd < 0) throw std::runtime_error("Could not open output file for appending");
    
    in.seekg(data_offset, std::ios::beg);
    const size_t buf_size = 1024 * 1024;
    std::vector<char> buffer(buf_size);
    
    size_t bytes_remaining = data_size;
    while (bytes_remaining > 0 && in) {
        size_t to_read = std::min(bytes_remaining, buf_size);
        in.read(buffer.data(), to_read);
        size_t bytes_read = in.gcount();
        if (bytes_read == 0) break;
        
        size_t written = 0;
        while (written < bytes_read) {
            ssize_t res = write(out_fd, buffer.data() + written, bytes_read - written);
            if (res < 0) {
                close(out_fd);
                throw std::runtime_error("Failed writing data to output file");
            }
            written += res;
        }
        bytes_remaining -= bytes_read;
    }
    
    close(out_fd);
    spdlog::info("Conversion successful! Output written to {}", output_file);
    
    if (sigmf) {
        std::string meta_filename = output_file + ".sigmf-meta";
        generate_sigmf(meta_filename, output_file, final_format, final_rate, 512);
        spdlog::info("SigMF metadata generated at {}", meta_filename);
    }
}
