#ifndef RUBBERBAND_STUB_H
#define RUBBERBAND_STUB_H

#include <stdint.h>
#include <vector>

namespace RubberBand {

class RubberBandStretcher {
public:
    enum Options {
        OptionProcessOffline = 0,
        OptionProcessRealTime = 1
    };

    RubberBandStretcher(uint32_t srate, uint32_t channels, Options options = OptionProcessRealTime) {}
    virtual ~RubberBandStretcher() {}

    void setTimeRatio(double ratio) {}
    void setPitchScale(double scale) {}
    
    void process(const float* const* input, uint32_t samples, bool final) {}
    uint32_t available() const { return 0; }
    uint32_t retrieve(float* const* output, uint32_t samples) { return 0; }
    
    void reset() {}
    uint32_t getLatency() const { return 0; }
};

}
#endif
