#include "serato_waveform.h"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace serato {

// Helper to handle Big Endian conversion
static uint32_t readUint32BE(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           (static_cast<uint32_t>(data[3]));
}

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::vector<uint8_t> base64_decode(std::string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<uint8_t> ret;

    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
    }

    return ret;
}

bool WaveformParser::parseBase64(const std::string& base64Data, SeratoAnalysis& out) {
    std::string cleanData = base64Data;
    // Remove potential "application/octet-stream" or similar prefixes if they exist (though rare in pure tags)
    // Most Serato Analysis tags are raw base64.
    
    std::vector<uint8_t> binary = base64_decode(cleanData);
    if (binary.empty()) return false;
    
    return parseBinary(binary, out);
}

bool WaveformParser::parseBinary(const std::vector<uint8_t>& data, SeratoAnalysis& out) {
    size_t offset = 0;
    while (offset + 8 <= data.size()) {
        const uint8_t* blockStart = &data[offset];
        char blockName[5];
        memcpy(blockName, blockStart, 4);
        blockName[4] = '\0';
        
        uint32_t blockLength = readUint32BE(blockStart + 4);
        offset += 8;
        
        if (offset + blockLength > data.size()) {
            std::cerr << "Serato Waveform: Block " << blockName << " exceeds data size" << std::endl;
            break;
        }
        
        const uint8_t* body = &data[offset];
        
        if (strcmp(blockName, "vrsn") == 0) {
            out.version = std::string((const char*)body, blockLength);
        } else if (strcmp(blockName, "onvg") == 0) {
            out.overview.assign(body, body + blockLength);
            // Basic parsing of overview: 
            // Often it's a series of 1-byte or 3-byte color samples.
            // For now we just store raw.
        } else if (strcmp(blockName, "anlz") == 0) {
            out.detailedWaveform.assign(body, body + blockLength);
        } else if (strcmp(blockName, "MARK") == 0) {
            // Serato Markers2 entry
            if (blockLength >= 12) {
                Marker m;
                // Markers2 structure: [1:idx][4:timeBE][1:type][4:colorBE][1:nameLen][...:name]
                m.time = readUint32BE(body + 1);
                m.type = body[5]; // 0=Cue, 1=Loop
                m.color = readUint32BE(body + 6) >> 8;
                uint8_t nameLen = body[10];
                if (nameLen > 0 && 11 + nameLen <= blockLength) {
                    m.name = std::string((const char*)(body + 11), nameLen);
                }
                out.markers.push_back(m);
            }
        }
        
        offset += blockLength;
    }
    
    return true;
}


} // namespace serato
