#include "engine/enginebuffer.h"
#include "audio/engine.h"

extern "C" {

void* EngineBridge_Create() {
    return (void*)new EngineBuffer("group", nullptr, nullptr, nullptr, unx::audio::ChannelCount(2));
}

void EngineBridge_Destroy(void* instance) {
    if (instance) delete (EngineBuffer*)instance;
}

void EngineBridge_InitializeWithPCM(void* instance, float* pBuffer, uint32_t totalSamples, uint32_t sampleRate) {
    if (instance) {
        printf("[EngineBridge] Initializing with PCM: %p, TotalSamples: %u, SampleRate: %u Hz\n", pBuffer, totalSamples, sampleRate);
        ((EngineBuffer*)instance)->initializeWithPCM(pBuffer, totalSamples, sampleRate);
    }
}

void EngineBridge_Process(void* instance, float* pOut, uint32_t frames, double rate) {
    if (instance) {
        EngineBuffer* engine = (EngineBuffer*)instance;
        engine->setSpeed(rate);
        engine->process(pOut, (std::size_t)frames * 2);
    }
}

void EngineBridge_Seek(void* instance, double pos) {
    if (instance) {
        ((EngineBuffer*)instance)->seekAbs(unx::audio::FramePos(pos));
    }
}

}
