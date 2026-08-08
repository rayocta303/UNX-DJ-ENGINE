#include "core/midi/midi_led_helper.h"
#include <string.h>

void MidiLed_Reset(MidiLedCache *cache) {
    if (cache) {
        memset(cache, 0, sizeof(MidiLedCache));
    }
}

uint8_t MidiLed_CalcJogPosition(double posSec) {
    (void)posSec;
    return 0;
}

uint8_t MidiLed_CalcVuLevel(float peakL, float peakR, float fader, bool isCueActive) {
    (void)peakL; (void)peakR; (void)fader; (void)isCueActive;
    return 0;
}

void MidiLed_UpdateVUMeters(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime, bool forceRefresh) {
    (void)cache; (void)d1; (void)d2; (void)engine; (void)nowTime; (void)forceRefresh;
}

void MidiLed_Update(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime) {
    (void)cache; (void)d1; (void)d2; (void)engine; (void)nowTime;
}
