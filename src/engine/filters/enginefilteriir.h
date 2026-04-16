#pragma once
#include "engine/engineobject.h"

#define FIDSPEC_LENGTH 1024
enum { IIR_BP, IIR_LP, IIR_HP };

template<int Order, int Type>
class EngineFilterIIR : public EngineObject {
public:
    EngineFilterIIR() : m_startFromDry(false) {}
    virtual ~EngineFilterIIR() {}
    void process(CSAMPLE* pInOut, const std::size_t bufferSize) override {}
    
    virtual void setCoefs(const char* spec, int specLen, unx::audio::SampleRate sampleRate, double centerFreq) {}
    virtual void setCoefs2(unx::audio::SampleRate sampleRate, double centerFreq, double Q) {}

    
protected:
    bool m_startFromDry;
};


