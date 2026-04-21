#ifndef SERATO_WAVEFORM_H
#define SERATO_WAVEFORM_H

#include <vector>
#include <string>
#include <cstdint>

namespace serato {

struct WaveformSample {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha; // Often used for intensity/height
};

struct Marker {
    uint32_t type; // 0=Cue, 1=Loop
    uint32_t time; // ms
    uint32_t color; // RGB
    uint32_t endTime; // for loops
    std::string name;
};

struct SeratoAnalysis {
    std::string version;
    std::vector<uint8_t> overview; // onvg raw data
    std::vector<uint8_t> detailedWaveform; // anlz raw data
    
    // Parsed data
    std::vector<WaveformSample> overviewSamples;
    std::vector<Marker> markers;
};

class WaveformParser {
public:
    WaveformParser() = default;

    /**
     * Parse Serato Analysis data from a base64 encoded string
     * @param base64Data The base64 encoded "Serato Analysis" tag content
     * @return true if parsing was successful
     */
    bool parseBase64(const std::string& base64Data, SeratoAnalysis& out);

    /**
     * Parse Serato Analysis data from raw binary data
     * @param data The binary "Serato Analysis" tag content
     * @return true if parsing was successful
     */
    bool parseBinary(const std::vector<uint8_t>& data, SeratoAnalysis& out);

private:
    bool parseBlock(const uint8_t* data, size_t size, SeratoAnalysis& out);
};

} // namespace serato

#endif // SERATO_WAVEFORM_H
