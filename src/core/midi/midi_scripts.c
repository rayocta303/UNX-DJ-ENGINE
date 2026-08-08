#include "core/midi/midi_scripts.h"
#include "audio/engine.h"
#include "core/logic/control_object.h"
#include "core/midi/midi_handler.h"
#include "ui/player/player_state.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern AudioEngine *globalAudioEngine;

static uint8_t highResMSB[4] = {0, 0, 0, 0};
static uint8_t lastVuVal[4] = {0, 0, 0, 0};
static uint8_t lastMasterVuL = 0;
static uint8_t lastMasterVuR = 0;

void MIDI_UpdateVuMeters(AudioEngine *engine, bool forceSend) {
  if (!engine)
    return;

  // Gain multiplier to map linear engine audio peaks to Pioneer DDJ hardware
  // LED calibration
  const float VU_GAIN = 2.1f;

  // 1. Channel VU Meters (Deck 1 - 4)
  for (int i = 0; i < 4; i++) {
    float peak = 0.0f;

    if (i < MAX_DECKS) {
      DeckAudioState *audio = &engine->Decks[i];
      float rawPeak = fmaxf(audio->VuMeterL, audio->VuMeterR);
      float trimVal = (audio->Trim > 0.0f) ? audio->Trim : 1.0f;
      peak = rawPeak * trimVal * VU_GAIN;
      if (peak > 1.0f)
        peak = 1.0f;
      if (peak < 0.0f)
        peak = 0.0f;
    }

    // Pioneer VU meters: 0x00 to 0x7F (0 to 127)
    uint8_t midiVal = (uint8_t)(peak * 127.0f);

    if (forceSend || (midiVal != lastVuVal[i])) {
      // 0xB0 = Ch 1, 0xB1 = Ch 2, 0xB2 = Ch 3, 0xB3 = Ch 4
      uint8_t status = 0xB0 | (i & 0x0F);
      uint8_t cc = 0x02; // Pioneer Channel Level Meter CC
      MIDI_SendShortMsg(status, cc, midiVal);
      lastVuVal[i] = midiVal;
    }
  }

  // 2. Master VU Meters (Master L & R)
  float masterVol = (engine->MasterVolume > 0.0f) ? engine->MasterVolume : 1.0f;
  float masterL = engine->MasterVuL * masterVol * VU_GAIN;
  float masterR = engine->MasterVuR * masterVol * VU_GAIN;
  if (masterL > 1.0f)
    masterL = 1.0f;
  if (masterL < 0.0f)
    masterL = 0.0f;
  if (masterR > 1.0f)
    masterR = 1.0f;
  if (masterR < 0.0f)
    masterR = 0.0f;

  uint8_t mValL = (uint8_t)(masterL * 127.0f);
  uint8_t mValR = (uint8_t)(masterR * 127.0f);

  if (forceSend || (mValL != lastMasterVuL)) {
    MIDI_SendShortMsg(0xBA, 0x00, mValL);
    lastMasterVuL = mValL;
  }
  if (forceSend || (mValR != lastMasterVuR)) {
    MIDI_SendShortMsg(0xBA, 0x01, mValR);
    lastMasterVuR = mValR;
  }
}

void MIDI_ResetVuMeters(void) {
  for (int i = 0; i < 4; i++) {
    uint8_t status = 0xB0 | (i & 0x0F);
    uint8_t cc = 0x02;
    MIDI_SendShortMsg(status, cc, 0);
    lastVuVal[i] = 0;
  }
  MIDI_SendShortMsg(0xBA, 0x00, 0);
  MIDI_SendShortMsg(0xBA, 0x01, 0);
  lastMasterVuL = 0;
  lastMasterVuR = 0;
}

static uint8_t lastLoopInVal[4] = {0, 0, 0, 0};
static uint8_t lastLoopOutVal[4] = {0, 0, 0, 0};
static uint8_t lastReloopVal[4] = {0, 0, 0, 0};
static uint8_t lastPlayVal[4] = {0, 0, 0, 0};
static uint8_t lastCueVal[4] = {0, 0, 0, 0};
static uint8_t lastSyncVal[4] = {0, 0, 0, 0};
static uint8_t lastMasterTempoVal[4] = {0, 0, 0, 0};
static uint8_t lastPadVals[4][16] = {{0}};
static uint8_t lastJogRingVal[4] = {0, 0, 0, 0};
static uint8_t lastJogPosVal[4] = {0, 0, 0, 0};

// Blink state: toggles every BLINK_INTERVAL calls (60fps -> ~4Hz blink)
#define BLINK_INTERVAL 8
static int blinkCounter = 0;
static bool blinkPhase = false;     // current blink LED state
static bool lastBlinkPhase = false; // previous phase — detect transition

void MIDI_UpdateLoopAndPadLEDs(DeckState *d1, DeckState *d2,
                               AudioEngine *engine, bool forceSend) {
  DeckState *decks[4] = {d1, d2, NULL, NULL};
  uint8_t mainStatuses[4] = {0x90, 0x91, 0x92, 0x93};
  uint8_t padStatuses[4] = {0x97, 0x99, 0x98, 0x9A};

  // Advance blink timer — only Play/CUE LEDs are force-sent on phase change.
  // All other LEDs use normal change-detection to avoid MIDI flooding.
  blinkCounter++;
  if (blinkCounter >= BLINK_INTERVAL) {
    blinkCounter = 0;
    blinkPhase = !blinkPhase;
  }
  bool blinkChanged = (blinkPhase != lastBlinkPhase);
  lastBlinkPhase = blinkPhase;

  for (int i = 0; i < 4; i++) {
    DeckState *ds = decks[i];
    DeckAudioState *audio =
        (engine && i < MAX_DECKS) ? &engine->Decks[i] : NULL;

    uint8_t mainStatus = mainStatuses[i];
    uint8_t padStatus = padStatuses[i];

    // -------------------------------------------------------------
    // 1. LOOP BUTTON LEDs (Loop In, Loop Out, Reloop/Exit)
    // -------------------------------------------------------------
    bool isLooping = false;
    bool hasLoopIn = false;
    bool hasLoopOut = false;

    if (audio) {
      isLooping = audio->IsLooping;
      hasLoopIn = (audio->LoopStartPos > 0);
      hasLoopOut = (audio->LoopEndPos > 0);
    }
    if (ds) {
      if (ds->IsLooping)
        isLooping = true;
      if (ds->LoopAdjustIn)
        hasLoopIn = true;
      if (ds->LoopAdjustOut)
        hasLoopOut = true;
    }

    uint8_t loopInVal = (hasLoopIn || isLooping) ? 0x7F : 0x00;
    uint8_t loopOutVal = (hasLoopOut || isLooping) ? 0x7F : 0x00;
    uint8_t reloopVal = isLooping ? 0x7F : 0x00;

    if (forceSend || loopInVal != lastLoopInVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x10, loopInVal); // LOOP IN / 4 BEAT
      lastLoopInVal[i] = loopInVal;
    }
    if (forceSend || loopOutVal != lastLoopOutVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x11, loopOutVal); // LOOP OUT
      lastLoopOutVal[i] = loopOutVal;
    }
    if (forceSend || reloopVal != lastReloopVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x12, reloopVal); // RELOOP / EXIT
      lastReloopVal[i] = reloopVal;
    }

    // -------------------------------------------------------------
    // 2. PLAY / CUE / SYNC / MASTER TEMPO LEDs
    // -------------------------------------------------------------
    bool isPlaying = false;
    bool isCueHeld = false;
    bool isCueActive = false;
    bool isSync = false;
    bool isMasterTempo = false;
    bool hasTrackLoaded = false;

    if (audio) {
      isPlaying = audio->IsPlaying;
      isMasterTempo = audio->MasterTempoActive;
    }
    if (ds) {
      if (ds->IsPlaying)    isPlaying = true;
      if (ds->IsCueHeld)    isCueHeld = true;
      if (ds->IsCueActive)  isCueActive = true;
      if (ds->SyncMode > 0) isSync = true;
      if (ds->MasterTempo)  isMasterTempo = true;
      if (ds->LoadedTrack)  hasTrackLoaded = true;
    }
    if (audio && audio->TotalSamples > 0) hasTrackLoaded = true;

    // ---- PLAY LED ----
    // Solid ON  = playing
    // Blink     = paused with track loaded  (hardware visual cue)
    // OFF       = no track loaded
    uint8_t playVal;
    if (isPlaying) {
      playVal = 0x7F; // Solid green
    } else if (hasTrackLoaded) {
      playVal = blinkPhase ? 0x7F : 0x00; // Blink when paused
    } else {
      playVal = 0x00; // Off
    }

    // ---- CUE LED ----
    // Solid ON  = cue button held / track paused exactly at cue point
    // Dim ON    = track has cue set but not held
    // OFF       = no track / playing past cue
    uint8_t cueVal;
    if (isCueHeld) {
      cueVal = 0x7F; // Solid while held
    } else if (!isPlaying && isCueActive) {
      cueVal = blinkPhase ? 0x7F : 0x20; // Blink between bright and dim when paused at cue
    } else if (!isPlaying && hasTrackLoaded) {
      cueVal = 0x20; // Dim — track paused, cue available
    } else {
      cueVal = 0x00; // Off while playing or no track
    }

    uint8_t syncVal = isSync ? 0x7F : 0x00;
    uint8_t mtVal = isMasterTempo ? 0x7F : 0x00;

    // Play/CUE: resend when value changes OR blink phase toggles (max 2 msgs / 8 frames)
    if (forceSend || blinkChanged || playVal != lastPlayVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x0B, playVal); // PLAY / PAUSE
      lastPlayVal[i] = playVal;
    }
    if (forceSend || blinkChanged || cueVal != lastCueVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x0C, cueVal); // CUE
      lastCueVal[i] = cueVal;
    }
    // Sync & Master Tempo: pure change-detection, no blink needed
    if (forceSend || syncVal != lastSyncVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x58, syncVal); // SYNC
      lastSyncVal[i] = syncVal;
    }
    if (forceSend || mtVal != lastMasterTempoVal[i]) {
      MIDI_SendShortMsg(mainStatus, 0x1A, mtVal); // MASTER TEMPO
      lastMasterTempoVal[i] = mtVal;
    }

    // -------------------------------------------------------------
    // 3. PADS LEDs (HotCues 1-8 & Beat Loops)
    // -------------------------------------------------------------
    for (int p = 0; p < 8; p++) {
      uint8_t padVal = 0x00; // Default OFF

      if (ds && ds->LoadedTrack) {
        // Check HotCue presence for Pad p (ID = p + 1)
        for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
          if (ds->LoadedTrack->HotCues[h].ID == (unsigned int)(p + 1)) {
            padVal = 0x7F; // Lit up HotCue Pad
            break;
          }
        }
      }

      // HotCue Mode Pad LED (0x00 .. 0x07)
      if (forceSend || padVal != lastPadVals[i][p]) {
        MIDI_SendShortMsg(padStatus, (uint8_t)p, padVal);
        lastPadVals[i][p] = padVal;
      }

      // Beat Loop Mode Pad LED (0x30 .. 0x37)
      uint8_t beatLoopPadVal =
          (isLooping && p == 2) ? 0x7F : (padVal > 0 ? 0x20 : 0x00);
      if (forceSend || beatLoopPadVal != lastPadVals[i][p + 8]) {
        MIDI_SendShortMsg(padStatus, (uint8_t)(0x30 + p), beatLoopPadVal);
        lastPadVals[i][p + 8] = beatLoopPadVal;
      }
    }

    // -------------------------------------------------------------
    // 4. JOG RING LEDs (Jog Wheel Ring Illumination & Position Indicator)
    // -------------------------------------------------------------
    bool hasTrack = (ds && ds->LoadedTrack) || (audio && audio->TotalSamples > 0);
    bool isTouch = (ds && ds->IsTouching) || (audio && audio->IsTouching);

    // Jog Outer/Inner Ring LED (0x9F, data1 = deck index 0..3)
    uint8_t jogRingVal = 0x00;
    if (hasTrack) {
      jogRingVal = isTouch ? 0x7F : (isPlaying ? 0x7F : 0x40);
    }
    if (forceSend || jogRingVal != lastJogRingVal[i]) {
      MIDI_SendShortMsg(0x9F, (uint8_t)i, jogRingVal);
      lastJogRingVal[i] = jogRingVal;
    }

    // Jog Ring Position Rotation LED (0xBB, data1 = deck index 0..3, data2 = 1..72)
    uint8_t jogPos72 = 1;
    uint8_t jogPos127 = 0;

    if (hasTrack && audio && audio->SampleRate > 0) {
      double posSec = (double)audio->Position / (double)audio->SampleRate;
      double rotAngle = fmod(posSec / 1.8, 1.0); // 1.8 sec per 33 1/3 RPM revolution
      if (rotAngle < 0) rotAngle += 1.0;
      jogPos72 = 1 + (uint8_t)(rotAngle * 71.0f); // 1..72 range for Pioneer 0xBB ring
      if (jogPos72 > 72) jogPos72 = 72;
      jogPos127 = (uint8_t)(rotAngle * 127.0f);
    }

    if (forceSend || jogPos72 != lastJogPosVal[i]) {
      // Send 0xBB (Pioneer DDJ-FLX6 / DDJ-1000 Hardware Jog Ring Status)
      MIDI_SendShortMsg(0xBB, (uint8_t)i, jogPos72);
      
      // Also send CC 0x2A & 0x2B (Pioneer DDJ-400 / FLX4 CC Ring Status)
      uint8_t ccStatus = 0xB0 | (i & 0x0F);
      MIDI_SendShortMsg(ccStatus, 0x2A, jogPos127);
      MIDI_SendShortMsg(ccStatus, 0x2B, jogRingVal);

      lastJogPosVal[i] = jogPos72;
    }
  }
}

void MIDI_ResetAllLEDs(void) {
  MIDI_ResetVuMeters();

  uint8_t mainStatuses[4] = {0x90, 0x91, 0x92, 0x93};
  uint8_t padStatuses[4] = {0x97, 0x99, 0x98, 0x9A};

  for (int i = 0; i < 4; i++) {
    uint8_t ms = mainStatuses[i];
    uint8_t ps = padStatuses[i];

    MIDI_SendShortMsg(ms, 0x10, 0); // Loop In
    MIDI_SendShortMsg(ms, 0x11, 0); // Loop Out
    MIDI_SendShortMsg(ms, 0x12, 0); // Reloop/Exit
    MIDI_SendShortMsg(ms, 0x0B, 0); // Play
    MIDI_SendShortMsg(ms, 0x0C, 0); // Cue
    MIDI_SendShortMsg(ms, 0x58, 0); // Sync
    MIDI_SendShortMsg(ms, 0x1A, 0); // Master Tempo

    MIDI_SendShortMsg(0x9F, (uint8_t)i, 0); // Jog Ring OFF
    MIDI_SendShortMsg(0xBB, (uint8_t)i, 0); // Jog Pos 0xBB OFF
    MIDI_SendShortMsg(0xB0 | (i & 0x0F), 0x2A, 0); // Jog Pos CC OFF
    MIDI_SendShortMsg(0xB0 | (i & 0x0F), 0x2B, 0);

    lastLoopInVal[i] = 0;
    lastLoopOutVal[i] = 0;
    lastReloopVal[i] = 0;
    lastPlayVal[i] = 0;
    lastCueVal[i] = 0;
    lastSyncVal[i] = 0;
    lastMasterTempoVal[i] = 0;
    lastJogRingVal[i] = 0;
    lastJogPosVal[i] = 0;

    for (int p = 0; p < 8; p++) {
      MIDI_SendShortMsg(ps, (uint8_t)p, 0);
      MIDI_SendShortMsg(ps, (uint8_t)(0x30 + p), 0);
      lastPadVals[i][p] = 0;
      lastPadVals[i][p + 8] = 0;
    }
  }
}

void MIDI_ExecuteScript(MidiMapping *map, const char *function, uint8_t status,
                        uint8_t midino, uint8_t value) {
  (void)midino;
  int deck = (status & 0x0F) % 4;
  const char *groupNames[4] = {"[Channel1]", "[Channel2]", "[Channel3]",
                               "[Channel4]"};
  const char *group = groupNames[deck];
  int targetDeckIdx = deck % 2; // 0 for Deck A (Ch 1/3), 1 for Deck B (Ch 2/4)

  if (strstr(function, "shiftButton") || strstr(function, "shiftPressed")) {
    map->modifiers[0] = (value > 0); // Modifier 0 is Shift
    CO_SetValue(group, "shift", (value > 0) ? 1.0f : 0.0f);
  } else if (strstr(function, "jogTurn") || strstr(function, "jogSearch")) {
    float delta = (float)value - 64.0f;
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
          if (newStart < audio->LoopEndPos - 16.0) {
            audio->LoopStartPos = newStart;
            DeckAudio_SetLoop(audio, true, audio->LoopStartPos,
                              audio->LoopEndPos);
          }
        } else if (adjOut && *adjOut) {
          adjusting = true;
          double newEnd = audio->LoopEndPos + (delta * 500.0);
          if (newEnd > audio->LoopStartPos + 16.0) {
            audio->LoopEndPos = newEnd;
            DeckAudio_SetLoop(audio, true, audio->LoopStartPos,
                              audio->LoopEndPos);
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
      bool isSearch =
          (strstr(function, "jogSearch") != NULL) || map->modifiers[0];
      float scale = isSearch ? 2.0f : (touching ? 0.1f : 0.005f);
      CO_AddValue(group, "jog", delta * scale);
    }
  } else if (strstr(function, "jogTouch") || strstr(function, "JogTouch")) {
    bool touching = (value > 0);
    CO_SetValue(group, "touch", touching ? 1.0f : 0.001f);
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
      if (strstr(function, "HotCue") || strstr(function, "hotcueMode") ||
          midino == 0x1B)
        CO_SetValue(group, "padmode", 0.0f); // PAD_MODE_HOT_CUE (0)
      else if (strstr(function, "BeatLoop") ||
               strstr(function, "beatLoopMode") || midino == 0x6D)
        CO_SetValue(group, "padmode", 1.0f); // PAD_MODE_BEAT_LOOP (1)
      else if (strstr(function, "PadFX") || strstr(function, "padFX") ||
               strstr(function, "SlipLoop") || midino == 0x1E || midino == 0x6B)
        CO_SetValue(group, "padmode", 2.0f); // PAD_MODE_SLIP_LOOP (2)
      else if (strstr(function, "BeatJump") ||
               strstr(function, "beatJumpMode") || midino == 0x20)
        CO_SetValue(group, "padmode", 3.0f); // PAD_MODE_BEAT_JUMP (3)
      else if (strstr(function, "Sampler") || strstr(function, "samplerMode") ||
               strstr(function, "GateCue") || midino == 0x22)
        CO_SetValue(group, "padmode", 4.0f); // PAD_MODE_GATE_CUE (4)
      else if (strstr(function, "ReleaseFX") ||
               strstr(function, "keyShiftMode") ||
               strstr(function, "keyboardMode") || midino == 0x69 ||
               midino == 0x6F)
        CO_SetValue(group, "padmode", 5.0f); // PAD_MODE_RELEASE_FX (5)
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
        if (req)
          *req = true;
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
        if (req)
          *req = true;
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
  } else if (strstr(function, "headMix") || strstr(function, "headphone_mix") ||
             strstr(function, "headMixRotate")) {
    float normVal = (float)value / 127.0f;
    CO_SetValue("[Master]", "headphone_mix", normVal);
    CO_SetValue("[Master]", "headMix", normVal);
  } else if (strstr(function, "beatjumpPadPressed")) {
    if (value > 0) {
      static const double beatSizes[8] = {-1.0, 1.0, -2.0, 2.0,
                                          -4.0, 4.0, -8.0, 8.0};
      int padIdx = -1;
      if (midino >= 0x20 && midino <= 0x27)
        padIdx = midino - 0x20;
      if (padIdx >= 0 && padIdx < 8) {
        double beats = beatSizes[padIdx];
        if (beats < 0) {
          CO_SetValue(group, "beatjump_backward", 1.0f);
        } else {
          CO_SetValue(group, "beatjump_forward", 1.0f);
        }
        if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
          DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
          double bpm = (audio->BPM > 0.0) ? audio->BPM : 120.0;
          double sampleRate =
              (audio->SampleRate > 0) ? (double)audio->SampleRate : 44100.0;
          double jumpSamples = beats * sampleRate * (60.0 / bpm);
          audio->Position += jumpSamples;
          if (audio->Position < 0.0)
            audio->Position = 0.0;
          if (audio->TotalSamples > 0 &&
              audio->Position >= (double)audio->TotalSamples) {
            audio->Position = (double)(audio->TotalSamples - 1);
          }
        }
      }
    }
  } else if (strstr(function, "decreaseBeatjumpSizes")) {
    if (value > 0) {
      CO_SetValue(group, "beatjump_backward", 1.0f);
      if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
        DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
        double bpm = (audio->BPM > 0.0) ? audio->BPM : 120.0;
        double sampleRate =
            (audio->SampleRate > 0) ? (double)audio->SampleRate : 44100.0;
        double jumpSamples = -16.0 * sampleRate * (60.0 / bpm);
        audio->Position += jumpSamples;
        if (audio->Position < 0.0)
          audio->Position = 0.0;
      }
    }
  } else if (strstr(function, "increaseBeatjumpSizes")) {
    if (value > 0) {
      CO_SetValue(group, "beatjump_forward", 1.0f);
      if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
        DeckAudioState *audio = &globalAudioEngine->Decks[targetDeckIdx];
        double bpm = (audio->BPM > 0.0) ? audio->BPM : 120.0;
        double sampleRate =
            (audio->SampleRate > 0) ? (double)audio->SampleRate : 44100.0;
        double jumpSamples = 16.0 * sampleRate * (60.0 / bpm);
        audio->Position += jumpSamples;
        if (audio->TotalSamples > 0 &&
            audio->Position >= (double)audio->TotalSamples) {
          audio->Position = (double)(audio->TotalSamples - 1);
        }
      }
    }
  } else if (strstr(function, "deckControlLPressed")) {
    if (value > 0) {
      CO_ToggleValue("[Channel1]", "deck_layer");
    }
  } else if (strstr(function, "deckControlRPressed")) {
    if (value > 0) {
      CO_ToggleValue("[Channel2]", "deck_layer");
    }
  } else if (strstr(function, "setGroupKeyValue") ||
             strstr(function, "keyboardButtonPressed")) {
    if (value > 0) {
      int semitone = 0;
      if (midino >= 0x70 && midino <= 0x77) {
        semitone = (int)midino - 0x74; // 0x74 is 0 semitones (reset pitch)
      } else if (midino >= 0x40 && midino <= 0x47) {
        semitone = (int)midino - 0x44;
      }
      CO_SetValue(group, "key_shift", (float)semitone);
    }
  } else if (strstr(function, "MoveVertical") ||
             strstr(function, "scrollTrack")) {
    float diff = (value >= 64) ? (float)(value - 128) : (float)value;
    CO_AddValue("[Library]", "scroll", diff);
  }
}
