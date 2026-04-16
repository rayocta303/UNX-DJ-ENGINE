#pragma once
#include "engine/scalers/enginebufferscale.h"

class ReadAheadManager;
class EngineBufferScaleSTPrivate;

class EngineBufferScaleST : public EngineBufferScale {
public:
    EngineBufferScaleST(ReadAheadManager* pReadAheadManager);
    ~EngineBufferScaleST() override;

    void clear() override;
    double scaleBuffer(CSAMPLE* pOutputBuffer, SINT iOutputBufferSize) override;

protected:
    void onSignalChanged() override;

private:
    ReadAheadManager* m_pReadAheadManager;
    EngineBufferScaleSTPrivate* d;
};
