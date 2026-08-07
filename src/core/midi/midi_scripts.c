#include "core/midi/midi_scripts.h"
#include "audio/engine.h"
#include "core/logic/control_object.h"
#include <stdio.h>
#include <string.h>

extern AudioEngine *globalAudioEngine;

static uint8_t highResMSB[4] = {0, 0, 0, 0};

void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status,
                        uint8_t midino, uint8_t value) {
  (void)midino;
  int deck = (status & 0x0F) % 4;
  const char *groupNames[4] = {"[Channel1]", "[Channel2]", "[Channel1]",
                               "[Channel2]"};
  const char *group = groupNames[deck];
  int targetDeckIdx = deck % 2; // 0 for Deck A (Ch 1/3), 1 for Deck B (Ch 2/4)

  if (strstr(function, "shiftButton") || strstr(function, "shiftPressed")) {
    map->modifiers[0] = (value > 0); // Modifier 0 is Shift
    CO_SetValue(group, "shift", (value > 0) ? 1.0f : 0.0f);
  } else if (strstr(function, "jogTurn") || strstr(function, "jogSearch")) {
    float delta = (float)value - 64.0f;
    bool isSearch = (strstr(function, "jogSearch") != NULL);
    bool adjusting = false;

    if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
      DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
      if (audio->IsLooping) {
        COType t;
        bool *adjIn = (bool *)CO_Find(group, "loop_adjust_in", &t);
        bool *adjOut = (bool *)CO_Find(group, "loop_adjust_out", &t);

        if (adjIn && *adjIn) {
          adjusting = true;
          double newStart = audio->LoopStartPos + (delta * 500.0);
          if (newStart < 0)
            newStart = 0;
          if (newStart < audio->LoopEndPos - 4410) {
            audio->LoopStartPos = newStart;
          }
        } else if (adjOut && *adjOut) {
          adjusting = true;
          double newEnd = audio->LoopEndPos + (delta * 500.0);
          if (newEnd > audio->LoopStartPos + 4410) {
            audio->LoopEndPos = newEnd;
          }
        }
      }
    }

    if (!adjusting) {
      bool touching =
          (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2)
              ? (globalAudioEngine->Decks[targetDeckIdx].IsTouching &&
                 globalAudioEngine->Decks[targetDeckIdx].VinylModeEnabled)
              : false;
      // JOG ADJUST RPM MULTIPLIER
      // float scale = touching ? 0.08f : 0.010f;
      float scale = touching ? 0.25f : 0.01f;
      if (map->modifiers[0] || isSearch)
        scale = 1.0f;
      CO_AddValue(group, "jog", delta * scale);
    }
  } else if (strstr(function, "jogTouch") || strstr(function, "JogTouch")) {
    bool touching = (value > 0);
    CO_SetValue(group, "touch", touching ? 1.0f : 0.0f);
    if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
      DeckAudio_SetJogTouch(&globalAudioEngine->Decks[targetDeckIdx], touching);
    }
  } else if (strstr(function, "beatFxLeftPressed") ||
             strstr(function, "beatFxBeatLeft")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_beat_left", 1.0f);
  } else if (strstr(function, "beatFxRightPressed") ||
             strstr(function, "beatFxBeatRight")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_beat_right", 1.0f);
  } else if (strstr(function, "beatTap") || strstr(function, "beatFxTap")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_tap", 1.0f);
  } else if (strstr(function, "beatFxSelect") ||
             strstr(function, "beatFxNext")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_next", 1.0f);
  } else if (strstr(function, "beatFxPrev")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_prev", 1.0f);
  } else if (strstr(function, "beatFxLevelDepth") ||
             strstr(function, "beatFxDepth")) {
    float depth = (float)value / 127.0f;
    CO_SetValue("[Master]", "beatfx_drywet", depth);
  } else if (strstr(function, "beatFxChannel1")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_ch1", 1.0f);
  } else if (strstr(function, "beatFxChannel2")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_ch2", 1.0f);
  } else if (strstr(function, "beatFxChannel3")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_ch3", 1.0f);
  } else if (strstr(function, "beatFxChannel4")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_ch4", 1.0f);
  } else if (strstr(function, "beatFxMaster") ||
             strstr(function, "beatFxChannelMaster")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_chmaster", 1.0f);
  } else if (strstr(function, "fxEnabled") ||
             strstr(function, "beatFxOnOffPressed") ||
             strstr(function, "beatFxOnOff")) {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_toggle", 1.0f);
  } else if (strstr(function, "padMode") || strstr(function, "PadMode")) {
    if (value > 0) {
      if (strstr(function, "HotCue") || midino == 0x1B)
        CO_SetValue(group, "padmode", 0.0f);
      else if (strstr(function, "BeatLoop") || midino == 0x6D)
        CO_SetValue(group, "padmode", 1.0f);
      else if (strstr(function, "BeatJump") || midino == 0x20)
        CO_SetValue(group, "padmode", 2.0f);
      else if (strstr(function, "Sampler") || midino == 0x22)
        CO_SetValue(group, "padmode", 3.0f);
    }
  } else if (strstr(function, "samplerPadPressed")) {
    if (value > 0) {
      CO_SetValue("[Library]", (targetDeckIdx == 0) ? "loadA" : "loadB", 1.0f);
    }
  } else if (strstr(function, "toggleLoopAdjustIn")) {
    if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
      if (globalAudioEngine->Decks[targetDeckIdx].IsLooping) {
        COType t;
        bool *adjIn = (bool *)CO_Find(group, "loop_adjust_in", &t);
        bool *adjOut = (bool *)CO_Find(group, "loop_adjust_out", &t);
        if (adjIn && adjOut) {
          *adjIn = !(*adjIn);
          *adjOut = false;
        }
      }
    }
  } else if (strstr(function, "toggleLoopAdjustOut")) {
    if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
      if (globalAudioEngine->Decks[targetDeckIdx].IsLooping) {
        COType t;
        bool *adjIn = (bool *)CO_Find(group, "loop_adjust_in", &t);
        bool *adjOut = (bool *)CO_Find(group, "loop_adjust_out", &t);
        if (adjIn && adjOut) {
          *adjOut = !(*adjOut);
          *adjIn = false;
        }
      }
    }
  } else if (strstr(function, "cueLoopCallLeft")) {
    static bool callLeftPressed[2] = {false, false};
    if (targetDeckIdx >= 0 && targetDeckIdx < 2) {
      if (value > 0) {
        callLeftPressed[targetDeckIdx] = true;
      } else if (callLeftPressed[targetDeckIdx]) {
        callLeftPressed[targetDeckIdx] = false;
        COType t;
        bool *req = (bool *)CO_Find(group, "loop_halve", &t);
        if (req) *req = true;
      }
    }
  } else if (strstr(function, "cueLoopCallRight")) {
    static bool callRightPressed[2] = {false, false};
    if (targetDeckIdx >= 0 && targetDeckIdx < 2) {
      if (value > 0) {
        callRightPressed[targetDeckIdx] = true;
      } else if (callRightPressed[targetDeckIdx]) {
        callRightPressed[targetDeckIdx] = false;
        COType t;
        bool *req = (bool *)CO_Find(group, "loop_double", &t);
        if (req) *req = true;
      }
    }
  } else if (strstr(function, "tempoSliderMSB")) {
    if (deck >= 0 && deck < 4) {
      highResMSB[deck] = value;
    }
  } else if (strstr(function, "tempoSliderLSB")) {
    if (deck >= 0 && deck < 4) {
      uint16_t fullValue = (highResMSB[deck] << 7) | value;
      float rateVal = 1.0f - ((float)fullValue / 8192.0f);

      COType t;
      int *rangePtr = (int *)CO_Find(group, "tempo_range", &t);
      int rangeIdx = rangePtr ? *rangePtr : 1;

      float maxPercent = 10.0f;
      if (rangeIdx == 0)
        maxPercent = 6.0f;
      else if (rangeIdx == 1)
        maxPercent = 10.0f;
      else if (rangeIdx == 2)
        maxPercent = 16.0f;
      else if (rangeIdx == 3)
        maxPercent = 100.0f;

      float *tempoPtr = (float *)CO_Find(group, "tempo_percent", &t);
      if (tempoPtr) {
        *tempoPtr = rateVal * maxPercent;
      }
    }
  } else if (strstr(function, "cycleTempoRange")) {
    COType t;
    int *rangePtr = (int *)CO_Find(group, "tempo_range", &t);
    if (rangePtr) {
      *rangePtr = (*rangePtr + 1) % 4;
    }
  } else if (strstr(function, "syncPressed") ||
             strstr(function, "syncLongPressed")) {
    if (value > 0)
      CO_SetValue(group, "sync", 1.0f);
  } else if (strstr(function, "quantizeToggle")) {
    if (value > 0)
      CO_ToggleValue(group, "quantize");
  } else if (strstr(function, "slipToggle")) {
    if (value > 0)
      CO_ToggleValue(group, "slip");
  } else if (strstr(function, "mergeFxTurn")) {
    float delta = (value >= 64) ? (float)(value - 128) : (float)value;
    CO_AddValue("[Master]", "beatfx_drywet", delta * 0.02f);
  } else if (strstr(function, "mergeFxPressed")) {
    if (value > 0)
      CO_ToggleValue("[Master]", "beatfx_on");
  } else if (strstr(function, "loadSelectedTrack") ||
             strstr(function, "LoadSelectedTrack")) {
    CO_SetValue("[Library]", (targetDeckIdx == 0) ? "loadA" : "loadB", 1.0f);
  } else if (strstr(function, "browseClick") ||
             strstr(function, "browsePush") ||
             strstr(function, "SelectTrack") ||
             strstr(function, "DirectoryPush") ||
             strstr(function, "LibraryPush") || strstr(function, "knobClick") ||
             strstr(function, "browseToggle")) {
    if (value > 0)
      CO_SetValue("[Library]", "enter", 1.0f);
  } else if (strstr(function, "waveformZoom") ||
             strstr(function, "waveform_zoom") ||
             strstr(function, "zoomWaveform")) {
    int delta = 0;
    if (value == 1 || value == 0x01) {
      delta = 1;
    } else if (value == 127 || value == 0x7F) {
      delta = -1;
    } else if (value > 1 && value < 64) {
      delta = (int)value;
    } else if (value > 64 && value < 127) {
      delta = -((int)128 - (int)value);
    }
    if (delta != 0) {
      CO_AddValue("[Master]", "waveform_zoom_step", (float)delta);
    }
  }
}
