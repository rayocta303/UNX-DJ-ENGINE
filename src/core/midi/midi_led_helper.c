#include "core/midi/midi_led_helper.h"
#include "core/midi/midi_handler.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void MidiLed_Reset(MidiLedCache *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(MidiLedCache));
    memset(cache->lastPlay, 255, sizeof(cache->lastPlay));
    memset(cache->lastCue, 255, sizeof(cache->lastCue));
    memset(cache->lastVinyl, 255, sizeof(cache->lastVinyl));
    memset(cache->lastVu, 255, sizeof(cache->lastVu));
    memset(cache->lastJog, 255, sizeof(cache->lastJog));
    memset(cache->lastHotCue, 255, sizeof(cache->lastHotCue));
    cache->lastMasterL = 255;
    cache->lastMasterR = 255;
    cache->lastFxOn = 255;
    cache->resolvedMapping = NULL;
}

static void MidiLed_ResolveMappingAddresses(MidiLedCache *cache, const MidiMapping *map) {
    if (!cache) return;
    cache->resolvedMapping = map;
    
    for (int i = 0; i < 4; i++) {
        MidiDeckAddresses *addr = &cache->deckAddr[i];
        // Pioneer Standard Hardware MIDI Addresses (Mixxx script parity)
        addr->playStatus    = 0x90 + i;  addr->playNote   = 0x0B;
        addr->cueStatus     = 0x90 + i;  addr->cueNote    = 0x0C;
        addr->vinylStatus   = 0x90 + i;  addr->vinylNote  = 0x0E;
        addr->loopInStatus  = 0x90 + i;  addr->loopInNote = 0x10;
        addr->loopOutStatus = 0x90 + i;  addr->loopOutNote= 0x11;
        addr->reloopStatus  = 0x90 + i;  addr->reloopNote = 0x4D;
        addr->vuStatus      = 0xB0 + i;  addr->vuControl  = 0x02;

        if (map) {
            const char *groupNames[4] = {"[Channel1]", "[Channel2]", "[Channel3]", "[Channel4]"};
            const char *group = groupNames[i];
            uint8_t s, m;
            if (MIDI_GetRegisterAddress(map, group, "vuMeterUpdate", &s, &m) ||
                MIDI_GetRegisterAddress(map, group, "vu", &s, &m)) {
                addr->vuStatus = s; addr->vuControl = m;
            }
            if (MIDI_GetRegisterAddress(map, group, "play", &s, &m)) {
                addr->playStatus = s; addr->playNote = m;
            }
            if (MIDI_GetRegisterAddress(map, group, "cue_default", &s, &m) ||
                MIDI_GetRegisterAddress(map, group, "cue", &s, &m)) {
                addr->cueStatus = s; addr->cueNote = m;
            }
            if (MIDI_GetRegisterAddress(map, group, "vinyl", &s, &m)) {
                addr->vinylStatus = s; addr->vinylNote = m;
            }
            if (MIDI_GetRegisterAddress(map, group, "loop_in", &s, &m)) {
                addr->loopInStatus = s; addr->loopInNote = m;
            }
            if (MIDI_GetRegisterAddress(map, group, "loop_out", &s, &m)) {
                addr->loopOutStatus = s; addr->loopOutNote = m;
            }
            if (MIDI_GetRegisterAddress(map, group, "reloop", &s, &m)) {
                addr->reloopStatus = s; addr->reloopNote = m;
            }
        }
    }
}

/**
 * 72-segment Pioneer Jog Wheel Ring formula (Mixxx playPositionUpdate)
 * (posSec * 72 * 0.6075) % 72 + 1
 */
uint8_t MidiLed_CalcJogPosition(double posSec) {
    if (posSec < 0.0) posSec = 0.0;
    double jogStep = fmod(posSec * 72.0 * 0.6075, 72.0);
    if (jogStep < 0.0) jogStep += 72.0;
    uint8_t pos = 0x01 + (uint8_t)jogStep;
    if (pos > 0x48) pos = 0x48;
    return pos;
}

/**
 * Mixxx vuMeterUpdate formula: value * 127
 */
uint8_t MidiLed_CalcVuLevel(float peakL, float peakR, float fader, bool isCueActive) {
    float rawPeak = (peakL > peakR) ? peakL : peakR;
    float chanVu = (fader > 0.01f) ? (rawPeak * fader) : (isCueActive ? rawPeak : (rawPeak * fader));
    int val = (int)(chanVu * 127.0f + 0.5f);
    if (val < 0) val = 0;
    if (val > 127) val = 127;
    return (uint8_t)val;
}

void MidiLed_UpdateVUMeters(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime, bool forceRefresh) {
    (void)nowTime;
    if (!cache || !engine) return;

    MidiMapping *map = MIDI_GetGlobalMapping();
    if (map != (const MidiMapping *)cache->resolvedMapping) {
        MidiLed_ResolveMappingAddresses(cache, map);
    }

    // 1. Channel 1 & 2 VU Meters (Mixxx vuMeterUpdate port)
    for (int i = 0; i < 2; i++) {
        DeckAudioState *deckAudio = &engine->Decks[i];
        DeckState *deckState = (i == 0) ? d1 : d2;

        uint8_t vuStatus = cache->deckAddr[i].vuStatus;
        uint8_t vuControl = cache->deckAddr[i].vuControl;

        bool cueActive = deckState ? deckState->IsCueActive : false;
        uint8_t meterVal = MidiLed_CalcVuLevel(deckAudio->VuMeterL, deckAudio->VuMeterR, deckAudio->Fader, cueActive);

        if (forceRefresh || meterVal != cache->lastVu[i]) {
            MIDI_SendShortMsg(vuStatus, vuControl, meterVal);
            cache->lastVu[i] = meterVal;
        }
    }

    // 2. Master VU Meters (Master L/R Mixxx port)
    uint8_t mL = (uint8_t)(engine->MasterVuL * 127.0f);
    uint8_t mR = (uint8_t)(engine->MasterVuR * 127.0f);
    if (mL > 127) mL = 127;
    if (mR > 127) mR = 127;

    if (forceRefresh || mL != cache->lastMasterL) {
        MIDI_SendShortMsg(0xBA, 0x00, mL);
        cache->lastMasterL = mL;
    }
    if (forceRefresh || mR != cache->lastMasterR) {
        MIDI_SendShortMsg(0xBA, 0x01, mR);
        cache->lastMasterR = mR;
    }
}

void MidiLed_Update(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime) {
    if (!cache || !d1 || !d2) return;

    MidiMapping *map = MIDI_GetGlobalMapping();
    if (map != (const MidiMapping *)cache->resolvedMapping) {
        MidiLed_ResolveMappingAddresses(cache, map);
    }

    // 1. Connection Handshake (Mixxx startup handshake)
    if (!cache->connectionHandshakeDone) {
        cache->connectionHandshakeDone = true;
        MidiLed_Reset(cache);
        MIDI_SendShortMsg(0x9F, 0x00, 0x7F); // Wakeup master hardware driver
        MIDI_SendShortMsg(0x90, 0x7F, 0x7F); // Init Ch 1
        MIDI_SendShortMsg(0x91, 0x7F, 0x7F); // Init Ch 2
    }

    // 2. Blink timer (300ms Mixxx LED blink cycle)
    if (nowTime - cache->lastBlinkTime > 0.300) {
        cache->lastBlinkTime = nowTime;
        cache->blinkState = !cache->blinkState;
    }

    // 3. Periodic full refresh every 3 seconds
    bool forceRefresh = false;
    if (cache->lastFullRefresh == 0 || nowTime - cache->lastFullRefresh > 3.0) {
        cache->lastFullRefresh = nowTime;
        forceRefresh = true;
    }

    // 4. Pioneer SysEx Keep-Alive every 1.5 seconds (Wireshark reverse-engineered Pioneer FLX6 keep-alive)
    if (nowTime - cache->lastSysExTime > 1.5) {
        cache->lastSysExTime = nowTime;
        static const uint8_t PIONEER_SYSEX_KEEPALIVE[12] = {
            0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7};
        MIDI_SendSysEx(PIONEER_SYSEX_KEEPALIVE, 12);
    }

    // 5. PRIORITY 1: Update Channel 1 & 2 & Master VU Meters (Evaluated every frame for 1:1 sync with UI)
    MidiLed_UpdateVUMeters(cache, d1, d2, engine, nowTime, forceRefresh);

    // 6. Rate Limit for button LEDs and jog ring (~60 FPS / 16ms delta)
    if (!forceRefresh && (nowTime - cache->lastSendTime < 0.016)) return;
    cache->lastSendTime = nowTime;

    // 7. Deck 1 & 2 Transport & Pad Signal Mapping (Channels 1 & 2 only)
    for (int i = 0; i < 2; i++) {
        DeckState *deck = (i == 0) ? d1 : d2;
        if (!deck) continue;

        if (deck->LoadedTrack != cache->lastLoadedTrack[i]) {
            cache->lastLoadedTrack[i] = deck->LoadedTrack;
            if (deck->LoadedTrack != NULL) {
                MIDI_SendShortMsg(0x9F, (uint8_t)i, 0x7F);
            }
        }

        MidiDeckAddresses *addr = &cache->deckAddr[i];

        // Play LED
        uint8_t playVal = 0x00;
        if (deck->IsPlaying) {
            playVal = 0x7F;
        } else if (deck->PositionMs <= deck->MainCueMs + 50 && deck->LoadedTrack) {
            playVal = cache->blinkState ? 0x7F : 0x00;
        }
        if (forceRefresh || playVal != cache->lastPlay[i]) {
            MIDI_SendShortMsg(addr->playStatus, addr->playNote, playVal);
            cache->lastPlay[i] = playVal;
        }

        // Cue LED
        uint8_t cueVal = 0x00;
        if (deck->IsCueActive || deck->IsCueHeld ||
            (!deck->IsPlaying && deck->PositionMs <= deck->MainCueMs + 50)) {
            cueVal = 0x7F;
        } else if (deck->IsPlaying && deck->LoadedTrack) {
            cueVal = cache->blinkState ? 0x7F : 0x00;
        }
        if (forceRefresh || cueVal != cache->lastCue[i]) {
            MIDI_SendShortMsg(addr->cueStatus, addr->cueNote, cueVal);
            cache->lastCue[i] = cueVal;
        }

        // Vinyl LED
        uint8_t vinylVal = deck->VinylModeEnabled ? 0x7F : 0x00;
        if (forceRefresh || vinylVal != cache->lastVinyl[i]) {
            MIDI_SendShortMsg(addr->vinylStatus, addr->vinylNote, vinylVal);
            cache->lastVinyl[i] = vinylVal;
        }

        // Loop LEDs
        uint8_t loopVal = 0x00;
        if (deck->LoopAdjustIn || deck->LoopAdjustOut) {
            loopVal = cache->blinkState ? 0x7F : 0x00;
        } else if (deck->IsLooping) {
            loopVal = 0x7F;
        }
        MIDI_SendShortMsg(addr->loopInStatus, addr->loopInNote, loopVal);
        MIDI_SendShortMsg(addr->loopOutStatus, addr->loopOutNote, loopVal);
        MIDI_SendShortMsg(addr->loopInStatus, 0x4C, loopVal);
        MIDI_SendShortMsg(addr->loopOutStatus, 0x4E, loopVal);
        MIDI_SendShortMsg(addr->reloopStatus, addr->reloopNote, deck->IsLooping ? 0x7F : 0x00);

        // HotCue Pad LEDs (Deck 1=0x97, Deck 2=0x99)
        uint8_t padStatus = (i == 0) ? 0x97 : 0x99;
        for (int p = 0; p < 8; p++) {
            uint8_t padNote = (uint8_t)p;
            bool hasHotCue = false;
            if (deck->LoadedTrack) {
                for (int h = 0; h < deck->LoadedTrack->HotCuesCount; h++) {
                    if (deck->LoadedTrack->HotCues[h].ID == (unsigned int)(p + 1)) {
                        hasHotCue = true; break;
                    }
                }
                if (!hasHotCue && deck->LoadedTrack->Analysis.CueCount > 0) {
                    for (uint32_t c = 0; c < deck->LoadedTrack->Analysis.CueCount; c++) {
                        if (deck->LoadedTrack->Analysis.Cues[c].ID == (unsigned int)(p + 1)) {
                            hasHotCue = true; break;
                        }
                    }
                }
            }
            uint8_t padVal = hasHotCue ? 0x7F : 0x00;
            if (forceRefresh || padVal != cache->lastHotCue[i][p]) {
                MIDI_SendShortMsg(padStatus, padNote, padVal);
                MIDI_SendShortMsg(padStatus, 0x30 + padNote, padVal);
                cache->lastHotCue[i][p] = padVal;
            }
        }
    }

    // 8. Beat FX On/Off LED
    if (engine) {
        uint8_t fxVal = engine->BeatFX.isFxOn ? 0x7F : 0x00;
        if (forceRefresh || fxVal != cache->lastFxOn) {
            MIDI_SendShortMsg(0x94, 0x47, fxVal);
            MIDI_SendShortMsg(0x94, 0x43, fxVal);
            cache->lastFxOn = fxVal;
        }
    }

    // 9. Jog Wheel Rings (Mixxx 72-segment position calculation)
    for (int i = 0; i < 2; i++) {
        DeckState *deck = (i == 0) ? d1 : d2;
        if (!deck) continue;

        double posSec = (double)deck->PositionMs / 1000.0;
        if (posSec <= 0.0001 && engine) {
            posSec = engine->Decks[i].Position / (double)(engine->Decks[i].SampleRate ? engine->Decks[i].SampleRate : 44100);
        }

        uint8_t pos = MidiLed_CalcJogPosition(posSec);
        if (forceRefresh || pos != cache->lastJog[i]) {
            MIDI_SendShortMsg(0xBB, (uint8_t)i, pos);
            cache->lastJog[i] = pos;
        }
    }
}
