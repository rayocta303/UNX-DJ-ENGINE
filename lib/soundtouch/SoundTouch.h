#ifndef SOUNDTOUCH_STUB_H
#define SOUNDTOUCH_STUB_H

#include <stdint.h>
#include <vector>

namespace soundtouch {

class SoundTouch {
public:
    SoundTouch();
    virtual ~SoundTouch();

    void setRate(double newRate);
    void setTempo(double newTempo);
    void setPitch(double newPitch);
    void setSampleRate(uint32_t srate);
    void setChannels(uint32_t channels);
    void setSetting(int settingId, int value);

    void clear();
    void putSamples(const float* samples, uint32_t nSamples);
    uint32_t receiveSamples(float* output, uint32_t maxSamples);

    static uint32_t getVersionId();

private:
    double m_rate;
    double m_tempo;
    double m_pitch;
    uint32_t m_sampleRate;
    uint32_t m_channels;

    std::vector<float> m_inputBuffer;
    double m_offset;
    double m_phaseOffset[2];
    bool m_searchTrigger[2];
    uint32_t m_framesProcessed;
};

#define SETTING_USE_QUICKSEEK 1

} // namespace soundtouch

#endif
