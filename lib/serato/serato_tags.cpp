#include "serato_tags.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace serato {

static uint32_t readUint32Synchsafe(const char* buf) {
    return (uint32_t)((unsigned char)buf[0] & 0x7F) << 21 |
           (uint32_t)((unsigned char)buf[1] & 0x7F) << 14 |
           (uint32_t)((unsigned char)buf[2] & 0x7F) << 7 |
           (uint32_t)((unsigned char)buf[3] & 0x7F);
}

static uint32_t readUint32BE(const char* buf) {
    return (uint32_t)((unsigned char)buf[0]) << 24 |
           (uint32_t)((unsigned char)buf[1]) << 16 |
           (uint32_t)((unsigned char)buf[2]) << 8 |
           (uint32_t)((unsigned char)buf[3]);
}

std::map<std::string, std::string> TagParser::readTags(const std::string& filePath) {
    std::map<std::string, std::string> tags;
    std::ifstream is(filePath, std::ios::binary);
    if (!is.is_open()) return tags;

    char header[10];
    is.read(header, 10);
    if (strncmp(header, "ID3", 3) != 0) return tags;

    uint32_t tagSize = readUint32Synchsafe(header + 6);
    uint32_t pos = 10;

    while (pos < tagSize + 10) {
        char frameHeader[10];
        is.read(frameHeader, 10);
        pos += 10;

        char frameId[5];
        memcpy(frameId, frameHeader, 4);
        frameId[4] = '\0';

        uint32_t frameSize = readUint32BE(frameHeader + 4);
        if (frameSize == 0 || frameSize > tagSize) break;

        if (strcmp(frameId, "GEOB") == 0 || strcmp(frameId, "COMM") == 0 || strcmp(frameId, "TXXX") == 0) {
            std::vector<char> buffer(frameSize);
            is.read(buffer.data(), frameSize);
            
            // Search for Serato identifiers in the buffer
            std::string content(buffer.data(), frameSize);
            
            if (strcmp(frameId, "GEOB") == 0) {
                // GEOB format: encoding (1) + mime (null term) + desc (null term) + data
                size_t mimeEnd = content.find('\0', 1);
                if (mimeEnd != std::string::npos) {
                    size_t descEnd = content.find('\0', mimeEnd + 1);
                    if (descEnd != std::string::npos) {
                        std::string desc = content.substr(mimeEnd + 1, descEnd - (mimeEnd + 1));
                        std::string data = content.substr(descEnd + 1);
                        if (desc.find("Serato Analysis") != std::string::npos) tags["Analysis"] = data;
                        else if (desc.find("Serato Markers") != std::string::npos) tags["Markers"] = data;
                    }
                }
            } else if (strcmp(frameId, "TXXX") == 0) {
                // TXXX format: encoding (1) + desc (null term) + value
                size_t descEnd = content.find('\0', 1);
                if (descEnd != std::string::npos) {
                    std::string desc = content.substr(1, descEnd - 1);
                    std::string value = content.substr(descEnd + 1);
                    if (desc.find("Serato Analysis") != std::string::npos) tags["Analysis"] = value;
                    else if (desc.find("Serato Markers") != std::string::npos) tags["Markers"] = value;
                }
            }
        } else {
            is.ignore(frameSize);
        }
        pos += frameSize;
    }

    return tags;
}

} // namespace serato
