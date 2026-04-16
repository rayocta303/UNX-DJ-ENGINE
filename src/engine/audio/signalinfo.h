#pragma once
#include "engine/audio/types.h"

namespace unx {
namespace audio {

class SignalInfo {
public:
    SignalInfo() : m_sampleRate(44100), m_channelCount(2) {}
    SignalInfo(ChannelCount cc, SampleRate sr) : m_sampleRate(sr), m_channelCount(cc) {}

    SampleRate getSampleRate() const { return m_sampleRate; }
    ChannelCount getChannelCount() const { return m_channelCount; }
    void setSampleRate(SampleRate sr) { m_sampleRate = sr; }
    void setChannelCount(ChannelCount cc) { m_channelCount = cc; }
    bool isValid() const { return m_sampleRate.isValid() && m_channelCount.isValid(); }

private:
    SampleRate m_sampleRate;
    ChannelCount m_channelCount;
};

} // namespace audio
} // namespace unx
