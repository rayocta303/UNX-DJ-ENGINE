#ifndef SOUNDTOUCH_STUB_H
#define SOUNDTOUCH_STUB_H

#include <stdint.h>

namespace soundtouch {

class SoundTouch {
public:
    SoundTouch() {}
    virtual ~SoundTouch() {}

    void setRate(double newRate) {}
    void setTempo(double newTempo) {}
    void setPitch(double newPitch) {}
    void setSampleRate(uint32_t srate) {}
    void setChannels(uint32_t channels) {}
    void setSetting(int settingId, int value) {}
    
    void clear() {}
    void putSamples(const float* samples, uint32_t nSamples) {}
    uint32_t receiveSamples(float* output, uint32_t maxSamples) { return 0; }
    
    static uint32_t getVersionId() { return 20302; }
};

#define SETTING_USE_QUICKSEEK 1

}
#endif
