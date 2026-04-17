#ifndef ENGINEBUFFERSCALEST_H
#define ENGINEBUFFERSCALEST_H

#include <memory>
#include "engine/scalers/enginebufferscale.h"
#include "engine/readaheadmanager.h"
#include "soundtouch/SoundTouch.h"

class EngineBufferScaleST : public EngineBufferScale {
public:
    explicit EngineBufferScaleST(ReadAheadManager* pReadAheadManager);
    ~EngineBufferScaleST() override;

    void setScaleParameters(double base_rate,
                            double* pTempoRatio,
                            double* pPitchRatio) override;

    double scaleBuffer(CSAMPLE* pOutputBuffer,
                       SINT iOutputBufferSize) override;

    void clear() override;

private:
    void onSignalChanged() override;

    ReadAheadManager* m_pReadAheadManager;
    std::unique_ptr<soundtouch::SoundTouch> m_pSoundTouch;
    CSAMPLE* m_bufferBack;
    uint32_t m_bufferBackSize;
    bool m_bBackwards;
};

#endif // ENGINEBUFFERSCALEST_H
