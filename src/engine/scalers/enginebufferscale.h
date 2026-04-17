#ifndef ENGINEBUFFERSCALE_H
#define ENGINEBUFFERSCALE_H

#include "engine/shim.h"
#include "engine/audio/types.h"
#include "engine/audio/signalinfo.h"

#define MAX_SEEK_SPEED 100.0
#define MIN_SEEK_SPEED 0.010

class EngineBufferScale {
public:
    EngineBufferScale();
    virtual ~EngineBufferScale() = default;

    const unx::audio::SignalInfo& getOutputSignal() const {
        return m_signal;
    }

    void setSignal(
            unx::audio::SampleRate sampleRate,
            unx::audio::ChannelCount channelCount);

    virtual void setScaleParameters(double base_rate,
                                    double* pTempoRatio,
                                    double* pPitchRatio) {
        m_dBaseRate = base_rate;
        m_dTempoRatio = *pTempoRatio;
        m_dPitchRatio = *pPitchRatio;
    }

    virtual void clear() = 0;
    
    virtual double scaleBuffer(
            CSAMPLE* pOutputBuffer,
            SINT iOutputBufferSize) = 0;

protected:
    virtual void onSignalChanged() = 0;

    unx::audio::SignalInfo m_signal;
    double m_dBaseRate;
    bool m_bSpeedAffectsPitch;
    double m_dTempoRatio;
    double m_dPitchRatio;
    double m_effectiveRate;
};

#endif // ENGINEBUFFERSCALE_H
