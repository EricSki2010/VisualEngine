#include "memory.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>

static std::string sMemoryPath;

void setMemoryPath(const std::string& dir) {
    sMemoryPath = dir;
}

// Parse a format file into ordered field names and bit widths
// Format: "fieldName -> bitCount" per line
static bool parseFormat(const std::string& formatPath,
                        std::vector<std::string>& fieldNames,
                        std::vector<int>& bitWidths) {
    std::ifstream file(formatPath);
    if (!file) {
        std::cerr << "saveToMemory: format file not found: " << formatPath << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        size_t arrow = line.find("->");
        if (arrow == std::string::npos) continue;

        std::string name = line.substr(0, arrow);
        std::string bits = line.substr(arrow + 2);

        // Trim whitespace
        while (!name.empty() && name.back() == ' ') name.pop_back();
        while (!bits.empty() && bits.front() == ' ') bits.erase(bits.begin());

        fieldNames.push_back(name);
        bitWidths.push_back(std::stoi(bits));
    }

    return !fieldNames.empty();
}

// Parse comma-separated values from a string
static std::vector<uint32_t> parseValues(const std::string& entry) {
    std::vector<uint32_t> values;
    std::stringstream ss(entry);
    std::string token;
    while (std::getline(ss, token, ',')) {
        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
        values.push_back((uint32_t)std::stoul(token));
    }
    return values;
}

// Write a value into a bit stream at the given bit offset
static void writeBits(std::vector<uint8_t>& buffer, int& bitOffset,
                      uint32_t value, int bitCount) {
    for (int i = bitCount - 1; i >= 0; i--) {
        int byteIndex = bitOffset / 8;
        int bitIndex = 7 - (bitOffset % 8);

        if (byteIndex >= (int)buffer.size())
            buffer.push_back(0);

        if (value & (1u << i))
            buffer[byteIndex] |= (1 << bitIndex);

        bitOffset++;
    }
}

// Pack a single entry's fields into one uint32_t
static uint32_t packEntry(const std::vector<uint32_t>& values,
                          const std::vector<int>& bitWidths) {
    uint32_t packed = 0;
    int shift = 0;
    for (int i = (int)values.size() - 1; i >= 0; i--) {
        packed |= (values[i] & ((1u << bitWidths[i]) - 1)) << shift;
        shift += bitWidths[i];
    }
    return packed;
}

// Unpack a uint32_t back into field values
static std::vector<uint32_t> unpackEntry(uint32_t packed,
                                         const std::vector<int>& bitWidths) {
    std::vector<uint32_t> values(bitWidths.size());
    int shift = 0;
    for (int i = (int)bitWidths.size() - 1; i >= 0; i--) {
        values[i] = (packed >> shift) & ((1u << bitWidths[i]) - 1);
        shift += bitWidths[i];
    }
    return values;
}

// RLE encode: consecutive identical packed values become [value, count] pairs
static std::vector<uint32_t> rleEncode(const std::vector<uint32_t>& packed) {
    std::vector<uint32_t> encoded;
    if (packed.empty()) return encoded;

    uint32_t current = packed[0];
    uint32_t count = 1;
    for (size_t i = 1; i < packed.size(); i++) {
        if (packed[i] == current && count < 0xFFFF) {
            count++;
        } else {
            encoded.push_back(current);
            encoded.push_back(count);
            current = packed[i];
            count = 1;
        }
    }
    encoded.push_back(current);
    encoded.push_back(count);
    return encoded;
}

// RLE decode: expand [value, count] pairs back into flat array
static std::vector<uint32_t> rleDecode(const std::vector<uint32_t>& encoded) {
    std::vector<uint32_t> packed;
    for (size_t i = 0; i + 1 < encoded.size(); i += 2) {
        uint32_t value = encoded[i];
        uint32_t count = encoded[i + 1];
        for (uint32_t c = 0; c < count; c++)
            packed.push_back(value);
    }
    return packed;
}

bool saveToMemory(const std::string& name, const std::string& formatPath,
                  const std::vector<std::string>& data) {
    std::vector<std::string> fieldNames;
    std::vector<int> bitWidths;
    if (!parseFormat(formatPath, fieldNames, bitWidths))
        return false;

    // Pack each entry into a uint32_t
    std::vector<uint32_t> packed;
    for (const auto& entry : data) {
        std::vector<uint32_t> values = parseValues(entry);
        if (values.size() != bitWidths.size()) {
            std::cerr << "saveToMemory: entry has " << values.size()
                      << " values, format expects " << bitWidths.size() << std::endl;
            return false;
        }
        packed.push_back(packEntry(values, bitWidths));
    }

    // RLE compress
    std::vector<uint32_t> encoded = rleEncode(packed);

    // Write binary file: [entryCount:32][rleSize:32][rle data...]
    std::string path = sMemoryPath + "/" + name + ".bin";
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "saveToMemory: failed to write: " << path << std::endl;
        return false;
    }

    uint32_t entryCount = (uint32_t)data.size();
    uint32_t rleSize = (uint32_t)encoded.size();
    out.write(reinterpret_cast<const char*>(&entryCount), 4);
    out.write(reinterpret_cast<const char*>(&rleSize), 4);
    out.write(reinterpret_cast<const char*>(encoded.data()), rleSize * 4);
    return true;
}

std::vector<std::string> loadFromMemory(const std::string& name, const std::string& formatPath) {
    std::vector<std::string> fieldNames;
    std::vector<int> bitWidths;
    if (!parseFormat(formatPath, fieldNames, bitWidths))
        return {};

    // Read binary file
    std::string path = sMemoryPath + "/" + name + ".bin";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "loadFromMemory: file not found: " << path << std::endl;
        return {};
    }

    // Read header
    uint32_t entryCount = 0, rleSize = 0;
    in.read(reinterpret_cast<char*>(&entryCount), 4);
    in.read(reinterpret_cast<char*>(&rleSize), 4);

    // Read RLE data
    std::vector<uint32_t> encoded(rleSize);
    in.read(reinterpret_cast<char*>(encoded.data()), rleSize * 4);

    // RLE decompress
    std::vector<uint32_t> packed = rleDecode(encoded);

    // Unpack each entry back to comma-separated string
    std::vector<std::string> result;
    for (uint32_t e = 0; e < entryCount && e < packed.size(); e++) {
        std::vector<uint32_t> values = unpackEntry(packed[e], bitWidths);
        std::string entry;
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) entry += ",";
            entry += std::to_string(values[i]);
        }
        result.push_back(entry);
    }

    return result;
}
