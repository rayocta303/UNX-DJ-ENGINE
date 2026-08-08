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
}

uint8_t MidiLed_CalcJogPosition(double posSec) {
    if (posSec < 0.0) posSec = 0.0;
    double jogStep = fmod(posSec * 72.0 * 0.6075, 72.0);
    if (jogStep < 0.0) jogStep += 72.0;
    uint8_t pos = 0x01 + (uint8_t)jogStep;
    if (pos > 0x48) pos = 0x48;
    return pos;
}

uint8_t MidiLed_CalcVuLevel(float peakL, float peakR, float fader, bool isCueActive) {
    float rawPeak = (peakL > peakR) ? peakL : peakR;
    float chanVu = (fader > 0.01f) ? (rawPeak * fader) : (isCueActive ? rawPeak : (rawPeak * fader));
    int val = (int)(chanVu * 127.0f + 0.5f);
    if (val < 0) val = 0;
    if (val > 127) val = 127;
    return (uint8_t)val;
}

void MidiLed_UpdateVUMeters(MidiLedCache *cache, DeckState *d1, DeckState *d2, AudioEngine *engine, double nowTime, bool forceRefresh) {
    (void)d1; (void)d2; (void)nowTime;
    if (!cache || !engine) return;

    MidiMapping *map = MIDI_GetGlobalMapping();

    // 1. Channel 1 & 2 VU Meters (Exact 1:1 parity with UI Mixer)
    for (int i = 0; i < 2; i++) {
        DeckAudioState *deckAudio = &engine->Decks[i];
        DeckState *deckState = (i == 0) ? d1 : d2;

        uint8_t vuStatus = 0xB0 + i;
        uint8_t vuControl = 0x02;

        if (map) {
            uint8_t s, m;
            const char *groupNames[2] = {"[Channel1]", "[Channel2]"};
            if (MIDI_GetRegisterAddress(map, groupNames[i], "vuMeterUpdate", &s, &m) ||
                MIDI_GetRegisterAddress(map, groupNames[i], "vu", &s, &m)) {
                vuStatus = s;
                vuControl = m;
            }
        }

        bool cueActive = deckState ? deckState->IsCueActive : false;
        uint8_t meterVal = MidiLed_CalcVuLevel(deckAudio->VuMeterL, deckAudio->VuMeterR, deckAudio->Fader, cueActive);

        // Immediate direct hardware dispatch on value change
        if (forceRefresh || meterVal != cache->lastVu[i]) {
            MIDI_SendShortMsg(vuStatus, vuControl, meterVal);
            cache->lastVu[i] = meterVal;
        }
    }

    // 2. Master VU Meters (Exact 1:1 parity with UI Master VU)
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

    // 1. Connection Handshake (Wakeup hardware drivers on connect - Channels 1 & 2)
    if (!cache->connectionHandshakeDone) {
        cache->connectionHandshakeDone = true;
        MidiLed_Reset(cache);
        MIDI_SendShortMsg(0x9F, 0x00, 0x7F); // Wakeup master hardware driver
        MIDI_SendShortMsg(0x90, 0x7F, 0x7F); // Init Ch 1
        MIDI_SendShortMsg(0x91, 0x7F, 0x7F); // Init Ch 2
    }

    // 2. Blink timer (300ms cycle)
    if (nowTime - cache->lastBlinkTime > 0.300) {
        cache->lastBlinkTime = nowTime;
        cache->blinkState = !cache->blinkState;
    }

    // 3. Periodic full refresh every 2.5 seconds
    bool forceRefresh = false;
    if (cache->lastFullRefresh == 0 || nowTime - cache->lastFullRefresh > 2.5) {
        cache->lastFullRefresh = nowTime;
        forceRefresh = true;
    }

    // 4. Pioneer SysEx Keep-Alive every 1.5 seconds
    if (nowTime - cache->lastSysExTime > 1.5) {
        cache->lastSysExTime = nowTime;
        static const uint8_t PIONEER_SYSEX_KEEPALIVE[12] = {
            0xF0, 0x00, 0x40, 0x05, 0x00, 0x00, 0x04, 0x05, 0x00, 0x50, 0x02, 0xF7};
        MIDI_SendSysEx(PIONEER_SYSEX_KEEPALIVE, 12);
    }

    // 5. PRIORITY 1: Update Channel 1 & 2 & Master VU Meters (Evaluated every frame in 100% sync with UI)
    MidiLed_UpdateVUMeters(cache, d1, d2, engine, nowTime, forceRefresh);

    // 6. Rate Limit for buttons and jog ring (~200 FPS = 0.005s delta)
    if (!forceRefresh && (nowTime - cache->lastSendTime < 0.005)) return;
    cache->lastSendTime = nowTime;

    MidiMapping *map = MIDI_GetGlobalMapping();

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

        const char *groupNames[2] = {"[Channel1]", "[Channel2]"};
        const char *group = groupNames[i];

        uint8_t playStatus = 0x90 + i, playNote = 0x0B;
        uint8_t cueStatus = 0x90 + i, cueNote = 0x0C;
        uint8_t vinylStatus = 0x90 + i, vinylNote = 0x0E;
        uint8_t loopInStatus = 0x90 + i, loopInNote = 0x10;
        uint8_t loopOutStatus = 0x90 + i, loopOutNote = 0x11;
        uint8_t reloopStatus = 0x90 + i, reloopNote = 0x4D;

        if (map) {
            uint8_t s, m;
            if (MIDI_GetRegisterAddress(map, group, "play", &s, &m)) { playStatus = s; playNote = m; }
            if (MIDI_GetRegisterAddress(map, group, "cue_default", &s, &m) ||
                MIDI_GetRegisterAddress(map, group, "cue", &s, &m)) { cueStatus = s; cueNote = m; }
            if (MIDI_GetRegisterAddress(map, group, "vinyl", &s, &m)) { vinylStatus = s; vinylNote = m; }
            if (MIDI_GetRegisterAddress(map, group, "loop_in", &s, &m)) { loopInStatus = s; loopInNote = m; }
            if (MIDI_GetRegisterAddress(map, group, "loop_out", &s, &m)) { loopOutStatus = s; loopOutNote = m; }
            if (MIDI_GetRegisterAddress(map, group, "reloop", &s, &m)) { reloopStatus = s; reloopNote = m; }
        }

        // Play LED
        uint8_t playVal = 0x00;
        if (deck->IsPlaying) {
            playVal = 0x7F;
        } else if (deck->PositionMs <= deck->MainCueMs + 50 && deck->LoadedTrack) {
            playVal = cache->blinkState ? 0x7F : 0x00;
        }
        if (forceRefresh || playVal != cache->lastPlay[i]) {
            MIDI_SendShortMsg(playStatus, playNote, playVal);
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
            MIDI_SendShortMsg(cueStatus, cueNote, cueVal);
            cache->lastCue[i] = cueVal;
        }

        // Vinyl LED
        uint8_t vinylVal = deck->VinylModeEnabled ? 0x7F : 0x00;
        if (forceRefresh || vinylVal != cache->lastVinyl[i]) {
            MIDI_SendShortMsg(vinylStatus, vinylNote, vinylVal);
            cache->lastVinyl[i] = vinylVal;
        }

        // Loop LEDs
        uint8_t loopVal = 0x00;
        if (deck->LoopAdjustIn || deck->LoopAdjustOut) {
            loopVal = cache->blinkState ? 0x7F : 0x00;
        } else if (deck->IsLooping) {
            loopVal = 0x7F;
        }
        MIDI_SendShortMsg(loopInStatus, loopInNote, loopVal);
        MIDI_SendShortMsg(loopOutStatus, loopOutNote, loopVal);
        MIDI_SendShortMsg(loopInStatus, 0x4C, loopVal);
        MIDI_SendShortMsg(loopOutStatus, 0x4E, loopVal);
        MIDI_SendShortMsg(reloopStatus, reloopNote, deck->IsLooping ? 0x7F : 0x00);

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

    // 9. Jog Wheel Rings (Decks 1 & 2 only)
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
