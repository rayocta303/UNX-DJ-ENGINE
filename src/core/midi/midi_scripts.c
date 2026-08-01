#include "core/midi/midi_scripts.h"
#include "core/logic/control_object.h"
#include "audio/engine.h"
#include <string.h>
#include <stdio.h>

extern AudioEngine *globalAudioEngine;

static uint8_t highResMSB[4] = {0, 0, 0, 0};

void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status, uint8_t midino, uint8_t value) {
    int deck = (status & 0x0F) % 4;
    const char *groupNames[4] = {"[Channel1]", "[Channel2]", "[Channel1]", "[Channel2]"};
    const char *group = groupNames[deck];
    int targetDeckIdx = deck % 2; // 0 for Deck A (Ch 1/3), 1 for Deck B (Ch 2/4)
    
    if (strstr(function, "shiftButton") || strstr(function, "shiftPressed")) {
        map->modifiers[0] = (value > 0); // Modifier 0 is Shift
        printf("[MIDI-SCRIPT] Shift %d: %s\n", deck + 1, map->modifiers[0] ? "ON" : "OFF");
    } 
    else if (strstr(function, "jogTurn") || strstr(function, "jogSearch")) {
        float delta = 0.0f;
        if (value >= 32 && value <= 96) {
            delta = (float)(value - 64);
        } else if (value < 32) {
            delta = (float)value;
        } else {
            delta = (float)(value - 128);
        }
        bool isSearch = (strstr(function, "jogSearch") != NULL);
        bool adjusting = false;
        
        if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
            DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
            if (audio->IsLooping) {
                COType t;
                bool *adjIn = (bool*)CO_Find(group, "loop_adjust_in", &t);
                bool *adjOut = (bool*)CO_Find(group, "loop_adjust_out", &t);
                
                if (adjIn && *adjIn) {
                    adjusting = true;
                    double newStart = audio->LoopStartPos + (delta * 500.0);
                    if (newStart < 0) newStart = 0;
                    if (newStart < audio->LoopEndPos - 4410) { // Keep min length 0.1s
                        audio->LoopStartPos = newStart;
                    }
                }
                else if (adjOut && *adjOut) {
                    adjusting = true;
                    double newEnd = audio->LoopEndPos + (delta * 500.0);
                    if (newEnd > audio->LoopStartPos + 4410) {
                        audio->LoopEndPos = newEnd;
                    }
                }
            }
        }
        
        if (!adjusting) {
            float scale = (map->modifiers[0] || isSearch) ? 5.0f : 1.0f;
            CO_AddValue(group, "jog", delta * scale);
        }
    }
    else if (strstr(function, "jogTouch")) {
        CO_SetValue(group, "touch", value > 0 ? 1.0f : 0.0f);
        printf("[MIDI-SCRIPT] Jog %d Touch: %d\n", deck + 1, value);
    }
    else if (strstr(function, "beatFxLeftPressed")) {
        if (value > 0) CO_SetValue("[Master]", "beatfx_prev", 1.0f);
    }
    else if (strstr(function, "beatFxRightPressed")) {
        if (value > 0) CO_SetValue("[Master]", "beatfx_next", 1.0f);
    }
    else if (strstr(function, "beatFxChannel1")) {
        CO_SetValue("[Master]", "beatfx_channel", 1.0f);
    }
    else if (strstr(function, "beatFxChannel2")) {
        CO_SetValue("[Master]", "beatfx_channel", 2.0f);
    }
    else if (strstr(function, "fxEnabled") || strstr(function, "beatFxOnOffPressed")) {
        if (value > 0) CO_SetValue("[Master]", "beatfx_toggle", 1.0f);
    }
    else if (strstr(function, "toggleLoopAdjustIn")) {
        if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
            if (globalAudioEngine->Decks[targetDeckIdx].IsLooping) {
                COType t;
                bool *adjIn = (bool*)CO_Find(group, "loop_adjust_in", &t);
                bool *adjOut = (bool*)CO_Find(group, "loop_adjust_out", &t);
                if (adjIn && adjOut) {
                    *adjIn = !(*adjIn);
                    *adjOut = false;
                    printf("[MIDI-SCRIPT] Loop Adjust In Deck %d: %s\n", targetDeckIdx + 1, *adjIn ? "ON" : "OFF");
                }
            }
        }
    }
    else if (strstr(function, "toggleLoopAdjustOut")) {
        if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
            if (globalAudioEngine->Decks[targetDeckIdx].IsLooping) {
                COType t;
                bool *adjIn = (bool*)CO_Find(group, "loop_adjust_in", &t);
                bool *adjOut = (bool*)CO_Find(group, "loop_adjust_out", &t);
                if (adjIn && adjOut) {
                    *adjOut = !(*adjOut);
                    *adjIn = false;
                    printf("[MIDI-SCRIPT] Loop Adjust Out Deck %d: %s\n", targetDeckIdx + 1, *adjOut ? "ON" : "OFF");
                }
            }
        }
    }
    else if (strstr(function, "cueLoopCallLeft")) {
        if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
            DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
            if (audio->IsLooping) {
                double loopLen = audio->LoopEndPos - audio->LoopStartPos;
                double newLen = loopLen * 0.5;
                if (newLen >= 4410) { // Limit min loop length to 0.1s
                    audio->LoopEndPos = audio->LoopStartPos + newLen;
                    printf("[MIDI-SCRIPT] Halve Loop Deck %d: new len = %.1f ms\n", targetDeckIdx + 1, (newLen * 1000.0) / 44100.0);
                }
            }
        }
    }
    else if (strstr(function, "cueLoopCallRight")) {
        if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
            DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
            if (audio->IsLooping) {
                double loopLen = audio->LoopEndPos - audio->LoopStartPos;
                double newLen = loopLen * 2.0;
                audio->LoopEndPos = audio->LoopStartPos + newLen;
                printf("[MIDI-SCRIPT] Double Loop Deck %d: new len = %.1f ms\n", targetDeckIdx + 1, (newLen * 1000.0) / 44100.0);
            }
        }
    }
    else if (strstr(function, "tempoSliderMSB")) {
        if (deck >= 0 && deck < 4) {
            highResMSB[deck] = value;
        }
    }
    else if (strstr(function, "tempoSliderLSB")) {
        if (deck >= 0 && deck < 4) {
            uint16_t fullValue = (highResMSB[deck] << 7) | value;
            float rateVal = 1.0f - ((float)fullValue / 8192.0f);
            
            COType t;
            int *rangePtr = (int*)CO_Find(group, "tempo_range", &t);
            int rangeIdx = rangePtr ? *rangePtr : 1;
            
            float maxPercent = 10.0f;
            if (rangeIdx == 0) maxPercent = 6.0f;
            else if (rangeIdx == 1) maxPercent = 10.0f;
            else if (rangeIdx == 2) maxPercent = 16.0f;
            else if (rangeIdx == 3) maxPercent = 100.0f;
            
            float *tempoPtr = (float*)CO_Find(group, "tempo_percent", &t);
            if (tempoPtr) {
                *tempoPtr = rateVal * maxPercent;
            }
        }
    }
    else if (strstr(function, "cycleTempoRange")) {
        COType t;
        int *rangePtr = (int*)CO_Find(group, "tempo_range", &t);
        if (rangePtr) {
            *rangePtr = (*rangePtr + 1) % 4;
            printf("[MIDI-SCRIPT] Cycle Tempo Range: %d\n", *rangePtr);
        }
    }
    else if (strstr(function, "syncPressed") || strstr(function, "syncLongPressed")) {
        if (value > 0) CO_SetValue(group, "sync", 1.0f);
    }
    else if (strstr(function, "quantizeToggle")) {
        if (value > 0) CO_ToggleValue(group, "quantize");
    }
    else if (strstr(function, "slipToggle")) {
        if (value > 0) CO_ToggleValue(group, "slip");
    }
    else if (strstr(function, "mergeFxTurn")) {
        float delta = (value >= 64) ? (float)(value - 128) : (float)value;
        CO_AddValue("[Master]", "beatfx_drywet", delta * 0.02f);
    }
    else if (strstr(function, "mergeFxPressed")) {
        if (value > 0) CO_ToggleValue("[Master]", "beatfx_on");
    }
    else if (strstr(function, "beatFxChannel3")) {
        CO_SetValue("[Master]", "beatfx_channel", 0.0f);
    }
    else if (strstr(function, "beatFxChannel4")) {
        CO_SetValue("[Master]", "beatfx_channel", 1.0f);
    }
    else if (strstr(function, "beatFxMaster")) {
        CO_SetValue("[Master]", "beatfx_channel", 2.0f);
    }
    else if (strstr(function, "loadSelectedTrack") || strstr(function, "LoadSelectedTrack")) {
        CO_SetValue("[Library]", (targetDeckIdx == 0) ? "loadA" : "loadB", 1.0f);
    }
}
