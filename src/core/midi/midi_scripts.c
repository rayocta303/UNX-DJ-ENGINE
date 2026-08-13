#include "core/midi/midi_scripts.h"
#include "audio/engine.h"
#include "core/logic/control_object.h"
#include "core/midi/midi_handler.h"
#include "ui/player/player_state.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern AudioEngine *globalAudioEngine;

static uint8_t highResMSB[4] = {0, 0, 0, 0};
static uint8_t lastVuVal[4] = {0, 0, 0, 0};
static uint8_t lastMasterVuL = 0;
static uint8_t lastMasterVuR = 0;

// =============================================================================
// DYNAMIC REGISTER TABLE — resolved from MidiMapping at runtime
// =============================================================================
// All MIDI output LED addresses (status + midino) are resolved from the loaded
// .midi.xml mapping file via MIDI_SetMappingRef(). Pioneer DDJ-FLX6 defaults
// are used as fallback when a key is not found in the mapping.

typedef struct {
  uint8_t status; // MIDI status byte (e.g. 0x90..0x9F, 0xB0..0xBB)
  uint8_t midino; // MIDI note/CC number
} MidiRegister;

// --- Per-deck transport & loop LEDs ---
static MidiRegister reg_play[4];
static MidiRegister reg_cue[4];
static MidiRegister reg_sync[4];
static MidiRegister reg_sync_leader[4];
static MidiRegister reg_master_tempo[4];
static MidiRegister reg_loop_in[4];
static MidiRegister reg_loop_out[4];
static MidiRegister reg_reloop[4];

// --- Per-deck toggle LEDs ---
static MidiRegister reg_slip[4];
static MidiRegister reg_keylock[4];
static MidiRegister reg_quantize[4];
static MidiRegister reg_pfl[4];

// --- Per-deck pad mode button LEDs ---
static MidiRegister reg_hotcue_mode[4];
static MidiRegister reg_beatloop_mode[4];
static MidiRegister reg_beatjump_mode[4];
static MidiRegister reg_sampler_mode[4];
static MidiRegister reg_padfx1_mode[4];
static MidiRegister reg_padfx2_mode[4];
static MidiRegister reg_keyboard_mode[4];
static MidiRegister reg_keyshift_mode[4];

// --- Per-deck pad LEDs ---
static MidiRegister
    reg_pad[4]; // base status for hotcue pads (midino = pad index 0..7)
static MidiRegister
    reg_beatloop[4]; // base status for beat loop pads (midino = 0x30 + index)

// --- Per-deck jog LEDs ---
static MidiRegister reg_jog_ring[4]; // 0x9F, deck index as midino
static MidiRegister reg_jog_pos[4];  // 0xBB, deck index as midino
static MidiRegister reg_jog_cc[4];   // 0xB0|deck, CC 0x2A / 0x2B

// --- VU meter registers ---
static MidiRegister
    reg_vu_ch[4]; // per-channel VU (status=0xB0|ch, midino=0x02)
static MidiRegister reg_vu_master_l; // master VU left  (0xBA, 0x00)
static MidiRegister reg_vu_master_r; // master VU right (0xBA, 0x01)

// --- Global / Beat FX & Master LEDs ---
static MidiRegister reg_beatfx_button; // 0x94, 0x47
static MidiRegister reg_beatfx_select; // 0x94, 0x63
static MidiRegister reg_release_fx;    // 0x94, 0x43
static MidiRegister reg_master_pfl;    // 0x94, 0x54
static MidiRegister reg_beatfx_ch[5];  // Ch 1, 2, 3, 4, Master
static MidiRegister reg_beat_left;     // BEAT LEFT (0x94, 0x06)
static MidiRegister reg_beat_right;    // BEAT RIGHT (0x94, 0x07)

static const char *kChannels[4] = {"[Channel1]", "[Channel2]", "[Channel3]",
                                   "[Channel4]"};

static void MIDI_BuildRegisterDefaults(void) {
  for (int i = 0; i < 4; i++) {
    uint8_t ms = (uint8_t)(0x90 | i);     // 0x90..0x93
    uint8_t ps = (uint8_t)(0x97 + i * 2); // 0x97, 0x99, 0x9B, 0x9D

    // Transport & Loop
    reg_play[i] = (MidiRegister){ms, 0x0B};
    reg_cue[i] = (MidiRegister){ms, 0x0C};
    reg_sync[i] = (MidiRegister){ms, 0x58};
    reg_sync_leader[i] = (MidiRegister){ms, 0x5C};
    reg_master_tempo[i] = (MidiRegister){ms, 0x1A};
    reg_loop_in[i] = (MidiRegister){ms, 0x10};
    reg_loop_out[i] = (MidiRegister){ms, 0x11};
    reg_reloop[i] = (MidiRegister){ms, 0x12};

    // Toggle buttons
    reg_slip[i] = (MidiRegister){ms, 0x3D};
    reg_keylock[i] = (MidiRegister){ms, 0x3E};
    reg_quantize[i] = (MidiRegister){ms, 0x5D};
    reg_pfl[i] = (MidiRegister){ms, 0x54};

    // Pad mode buttons
    reg_hotcue_mode[i] = (MidiRegister){ms, 0x1B};
    reg_beatloop_mode[i] = (MidiRegister){ms, 0x6D};
    reg_beatjump_mode[i] = (MidiRegister){ms, 0x20};
    reg_sampler_mode[i] = (MidiRegister){ms, 0x22};
    reg_padfx1_mode[i] = (MidiRegister){ms, 0x1E};
    reg_padfx2_mode[i] = (MidiRegister){ms, 0x6B};
    reg_keyboard_mode[i] = (MidiRegister){ms, 0x69};
    reg_keyshift_mode[i] = (MidiRegister){ms, 0x6F};

    // Pad LEDs
    reg_pad[i] = (MidiRegister){ps, 0x00};      // midino = pad index 0..7
    reg_beatloop[i] = (MidiRegister){ps, 0x30}; // midino = 0x30 + pad index

    // Jog wheel LEDs
    reg_jog_ring[i] = (MidiRegister){0x9F, (uint8_t)i};
    reg_jog_pos[i] = (MidiRegister){0xBB, (uint8_t)i};
    reg_jog_cc[i] = (MidiRegister){(uint8_t)(0xB0 | i), 0x2A};

    // VU meters
    reg_vu_ch[i] = (MidiRegister){(uint8_t)(0xB0 | i), 0x02};
  }
  reg_vu_master_l = (MidiRegister){0xBA, 0x00};
  reg_vu_master_r = (MidiRegister){0xBA, 0x01};
  reg_beatfx_button = (MidiRegister){0x94, 0x47};
  reg_beatfx_select = (MidiRegister){0x94, 0x63};
  reg_release_fx = (MidiRegister){0x94, 0x43};
  reg_master_pfl = (MidiRegister){0x94, 0x54};

  // Beat FX CH Selector (Ch1..Ch4, Master)
  reg_beatfx_ch[0] = (MidiRegister){0x94, 0x1C};
  reg_beatfx_ch[1] = (MidiRegister){0x94, 0x1D};
  reg_beatfx_ch[2] = (MidiRegister){0x94, 0x1E};
  reg_beatfx_ch[3] = (MidiRegister){0x94, 0x1F};
  reg_beatfx_ch[4] = (MidiRegister){0x94, 0x14};

  // Beat FX Beat Select
  reg_beat_left = (MidiRegister){0x94, 0x06};
  reg_beat_right = (MidiRegister){0x94, 0x07};
}

void MIDI_SetMappingRef(const MidiMapping *map) {
  MIDI_BuildRegisterDefaults();
  if (!map)
    return;

  for (int i = 0; i < 4; i++) {
    const char *grp = kChannels[i];
    uint8_t st = 0, no = 0;

#define TRY_RESOLVE(reg, key)                                                  \
  if (MIDI_GetRegisterAddress(map, grp, key, &st, &no)) {                      \
    (reg).status = st;                                                         \
    (reg).midino = no;                                                         \
  }

    // Transport & Loop
    TRY_RESOLVE(reg_play[i], "play")
    TRY_RESOLVE(reg_cue[i], "cue_point")
    TRY_RESOLVE(reg_sync[i], "sync_enabled")
    TRY_RESOLVE(reg_sync_leader[i], "sync_leader")
    TRY_RESOLVE(reg_master_tempo[i], "master_tempo")
    TRY_RESOLVE(reg_loop_in[i], "loop_in")
    TRY_RESOLVE(reg_loop_out[i], "loop_out")
    TRY_RESOLVE(reg_reloop[i], "reloop_exit")

    // Toggle buttons
    TRY_RESOLVE(reg_slip[i], "slip_enabled")
    TRY_RESOLVE(reg_keylock[i], "keylock")
    TRY_RESOLVE(reg_quantize[i], "quantize")
    TRY_RESOLVE(reg_pfl[i], "pfl")

    // Pad mode buttons
    TRY_RESOLVE(reg_hotcue_mode[i], "hotcueMode")
    TRY_RESOLVE(reg_beatloop_mode[i], "beatLoopMode")
    TRY_RESOLVE(reg_beatjump_mode[i], "beatJumpMode")
    TRY_RESOLVE(reg_sampler_mode[i], "samplerMode")
    TRY_RESOLVE(reg_padfx1_mode[i], "padFX1Mode")
    TRY_RESOLVE(reg_padfx2_mode[i], "padFX2Mode")
    TRY_RESOLVE(reg_keyboard_mode[i], "keyboardMode")
    TRY_RESOLVE(reg_keyshift_mode[i], "keyShiftMode")

    // Pad LEDs — only status byte resolved, midino = pad index at call site
    TRY_RESOLVE(reg_pad[i], "hotcue_1_activate")
    TRY_RESOLVE(reg_beatloop[i], "beatloop_0.5_toggle")

#undef TRY_RESOLVE
  }

  // Global & Master / Beat FX registers
  uint8_t st = 0, no = 0;
  if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatFxEnable", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1_Effect1]", "fxEnabled", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatfx_enable", &st, &no)) {
    reg_beatfx_button = (MidiRegister){st, no};
  }

  if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatFxSelect", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatfx_select", &st, &no)) {
    reg_beatfx_select = (MidiRegister){st, no};
  }

  if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "releaseFx", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "release_fx", &st, &no)) {
    reg_release_fx = (MidiRegister){st, no};
  }

  if (MIDI_GetRegisterAddress(map, "[Master]", "pfl", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[Master]", "headphone_cue", &st, &no)) {
    reg_master_pfl = (MidiRegister){st, no};
  }

  if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatFxLeft", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beat_left", &st, &no)) {
    reg_beat_left = (MidiRegister){st, no};
  }

  if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatFxRight", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beat_right", &st, &no)) {
    reg_beat_right = (MidiRegister){st, no};
  }

  // Resolving Beat FX CH Selector (Ch1..Ch4 & Master)
  char chKey[64];
  for (int c = 0; c < 4; c++) {
    snprintf(chKey, sizeof(chKey), "beatFxChannel%d", c + 1);
    if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", chKey, &st, &no) ||
        MIDI_GetRegisterAddress(map, kChannels[c], "beatfx_ch", &st, &no)) {
      reg_beatfx_ch[c] = (MidiRegister){st, no};
    }
  }
  if (MIDI_GetRegisterAddress(map, "[EffectRack1_EffectUnit1]", "beatFxChannelMaster", &st, &no) ||
      MIDI_GetRegisterAddress(map, "[Master]", "beatfx_ch", &st, &no)) {
    reg_beatfx_ch[4] = (MidiRegister){st, no};
  }


  printf("[MIDI] Register table resolved from mapping: '%s' (%d controls, %d "
         "outputs)\n",
         map->name, map->count, map->outputCount);
}

// =============================================================================

void MIDI_UpdateVuMeters(AudioEngine *engine, bool forceSend) {
  if (!engine)
    return;

  static bool vuInitialized = false;
  if (!vuInitialized) {
    MIDI_BuildRegisterDefaults();
    vuInitialized = true;
  }

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

    uint8_t midiVal = (uint8_t)(peak * 127.0f);
    if (forceSend || (midiVal != lastVuVal[i])) {
      MIDI_SendShortMsg(reg_vu_ch[i].status, reg_vu_ch[i].midino, midiVal);
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
    MIDI_SendShortMsg(reg_vu_master_l.status, reg_vu_master_l.midino, mValL);
    lastMasterVuL = mValL;
  }
  if (forceSend || (mValR != lastMasterVuR)) {
    MIDI_SendShortMsg(reg_vu_master_r.status, reg_vu_master_r.midino, mValR);
    lastMasterVuR = mValR;
  }
}

void MIDI_ResetVuMeters(void) {
  for (int i = 0; i < 4; i++) {
    MIDI_SendShortMsg(reg_vu_ch[i].status, reg_vu_ch[i].midino, 0);
    lastVuVal[i] = 0;
  }
  MIDI_SendShortMsg(reg_vu_master_l.status, reg_vu_master_l.midino, 0);
  MIDI_SendShortMsg(reg_vu_master_r.status, reg_vu_master_r.midino, 0);
  lastMasterVuL = 0;
  lastMasterVuR = 0;
}

// Last-state tracking arrays (change-detection to avoid MIDI flooding)
// =============================================================================
static uint8_t lastLoopInVal[4] = {0, 0, 0, 0};
static uint8_t lastLoopOutVal[4] = {0, 0, 0, 0};
static uint8_t lastReloopVal[4] = {0, 0, 0, 0};
static uint8_t lastPlayVal[4] = {0, 0, 0, 0};
static uint8_t lastCueVal[4] = {0, 0, 0, 0};
static uint8_t lastSyncVal[4] = {0, 0, 0, 0};
static uint8_t lastMasterTempoVal[4] = {0, 0, 0, 0};
static uint8_t lastSlipVal[4] = {0, 0, 0, 0};
static uint8_t lastKeylockVal[4] = {0, 0, 0, 0};
static uint8_t lastQuantizeVal[4] = {0, 0, 0, 0};
static uint8_t lastPflVal[4] = {0, 0, 0, 0};
static uint8_t lastSyncLeaderVal[4] = {0, 0, 0, 0};
static uint8_t lastHotcueModeVal[4] = {0, 0, 0, 0};
static uint8_t lastBeatloopModeVal[4] = {0, 0, 0, 0};
static uint8_t lastBeatjumpModeVal[4] = {0, 0, 0, 0};
static uint8_t lastSamplerModeVal[4] = {0, 0, 0, 0};
static uint8_t lastPadfx1ModeVal[4] = {0, 0, 0, 0};
static uint8_t lastPadfx2ModeVal[4] = {0, 0, 0, 0};
static uint8_t lastKeyboardModeVal[4] = {0, 0, 0, 0};
static uint8_t lastKeyshiftModeVal[4] = {0, 0, 0, 0};
static uint8_t lastPadVals[4][16] = {{0}};
static uint8_t lastJogRingVal[4] = {0, 0, 0, 0};
static uint8_t lastJogPosVal[4] = {0, 0, 0, 0};

// Global / Beat FX & Master LED last-state tracking
static uint8_t lastBeatFxButtonVal = 0;
static uint8_t lastBeatFxSelectVal = 0;
static uint8_t lastReleaseFxVal = 0;
static uint8_t lastMasterPflVal = 0;
static uint8_t lastBeatFxChVal[5] = {0, 0, 0, 0, 0};
static uint8_t lastBeatLeftVal = 0;
static uint8_t lastBeatRightVal = 0;

// Blink state: toggles every BLINK_INTERVAL calls (60fps -> ~4Hz blink)
#define BLINK_INTERVAL 8
static int blinkCounter = 0;
static bool blinkPhase = false;     // current blink LED state
static bool lastBlinkPhase = false; // previous phase — detect transition

// =============================================================================

void MIDI_UpdateLoopAndPadLEDs(DeckState *d1, DeckState *d2,
                               AudioEngine *engine, bool forceSend) {
  // Ensure register defaults exist (no-op if already initialized)
  static bool regInitialized = false;
  if (!regInitialized) {
    MIDI_BuildRegisterDefaults();
    regInitialized = true;
  }

  DeckState *decks[4] = {d1, d2, NULL, NULL};

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

    uint8_t mainStatus = reg_play[i].status; // resolved from mapping
    (void)mainStatus; // used via reg_* table directly below
    uint8_t padStatus = reg_pad[i].status; // resolved from mapping
    (void)padStatus; // used via reg_* table directly below

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
      MIDI_SendShortMsg(reg_loop_in[i].status, reg_loop_in[i].midino,
                        loopInVal);
      lastLoopInVal[i] = loopInVal;
    }
    if (forceSend || loopOutVal != lastLoopOutVal[i]) {
      MIDI_SendShortMsg(reg_loop_out[i].status, reg_loop_out[i].midino,
                        loopOutVal);
      lastLoopOutVal[i] = loopOutVal;
    }
    if (forceSend || reloopVal != lastReloopVal[i]) {
      MIDI_SendShortMsg(reg_reloop[i].status, reg_reloop[i].midino, reloopVal);
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
      if (ds->IsPlaying)
        isPlaying = true;
      if (ds->IsCueHeld)
        isCueHeld = true;
      if (ds->IsCueActive)
        isCueActive = true;
      if (ds->SyncMode > 0)
        isSync = true;
      if (ds->MasterTempo)
        isMasterTempo = true;
      if (ds->LoadedTrack)
        hasTrackLoaded = true;
    }
    if (audio && audio->TotalSamples > 0)
      hasTrackLoaded = true;

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
      cueVal = blinkPhase
                   ? 0x7F
                   : 0x20; // Blink between bright and dim when paused at cue
    } else if (!isPlaying && hasTrackLoaded) {
      cueVal = 0x20; // Dim — track paused, cue available
    } else {
      cueVal = 0x00; // Off while playing or no track
    }

    uint8_t syncVal = isSync ? 0x7F : 0x00;
    uint8_t mtVal = isMasterTempo ? 0x7F : 0x00;

    // ---- SLIP / KEYLOCK / QUANTIZE / PFL / SYNC LEADER ----
    bool isSlip = (audio && audio->SlipActive);
    uint8_t slipVal = isSlip ? 0x7F : 0x00;

    bool isQuantize = (ds && ds->QuantizeEnabled);
    uint8_t quantizeVal = isQuantize ? 0x7F : 0x00;

    COType coT;
    float *pflPtr = (float *)CO_Find(kChannels[i], "pfl", &coT);
    bool isPfl = pflPtr ? (*pflPtr > 0.0f) : false;
    uint8_t pflVal = isPfl ? 0x7F : 0x00;

    bool isMaster = (ds && ds->IsMaster);
    uint8_t syncLeaderVal = isMaster ? 0x7F : 0x00;

    // Play/CUE: resend when value changes OR blink phase toggles (max 2 msgs /
    // 8 frames)
    if (forceSend || blinkChanged || playVal != lastPlayVal[i]) {
      MIDI_SendShortMsg(reg_play[i].status, reg_play[i].midino, playVal);
      lastPlayVal[i] = playVal;
    }
    if (forceSend || blinkChanged || cueVal != lastCueVal[i]) {
      MIDI_SendShortMsg(reg_cue[i].status, reg_cue[i].midino, cueVal);
      lastCueVal[i] = cueVal;
    }
    if (forceSend || syncVal != lastSyncVal[i]) {
      MIDI_SendShortMsg(reg_sync[i].status, reg_sync[i].midino, syncVal);
      lastSyncVal[i] = syncVal;
    }
    if (forceSend || mtVal != lastMasterTempoVal[i]) {
      MIDI_SendShortMsg(reg_master_tempo[i].status, reg_master_tempo[i].midino,
                        mtVal);
      MIDI_SendShortMsg(reg_keylock[i].status, reg_keylock[i].midino, mtVal);
      lastMasterTempoVal[i] = mtVal;
      lastKeylockVal[i] = mtVal;
    }
    if (forceSend || slipVal != lastSlipVal[i]) {
      MIDI_SendShortMsg(reg_slip[i].status, reg_slip[i].midino, slipVal);
      lastSlipVal[i] = slipVal;
    }
    if (forceSend || quantizeVal != lastQuantizeVal[i]) {
      MIDI_SendShortMsg(reg_quantize[i].status, reg_quantize[i].midino,
                        quantizeVal);
      lastQuantizeVal[i] = quantizeVal;
    }
    if (forceSend || pflVal != lastPflVal[i]) {
      MIDI_SendShortMsg(reg_pfl[i].status, reg_pfl[i].midino, pflVal);
      lastPflVal[i] = pflVal;
    }
    if (forceSend || syncLeaderVal != lastSyncLeaderVal[i]) {
      MIDI_SendShortMsg(reg_sync_leader[i].status, reg_sync_leader[i].midino,
                        syncLeaderVal);
      lastSyncLeaderVal[i] = syncLeaderVal;
    }

    // Pad Mode Button LEDs — HotCue mode lit by default
    uint8_t hotcueModeVal = 0x7F;
    if (forceSend || hotcueModeVal != lastHotcueModeVal[i]) {
      MIDI_SendShortMsg(reg_hotcue_mode[i].status, reg_hotcue_mode[i].midino,
                        hotcueModeVal);
      lastHotcueModeVal[i] = hotcueModeVal;
    }

    // -------------------------------------------------------------
    // 3. PADS LEDs (HotCues 1-8 & Beat Loops)
    // -------------------------------------------------------------
    for (int p = 0; p < 8; p++) {
      uint8_t padVal = 0x00; // Default OFF

      if (ds && ds->LoadedTrack) {
        // Check HotCue presence for Pad p (ID = p + 1)
        for (int h = 0; h < ds->LoadedTrack->HotCuesCount; h++) {
          HotCue hc = ds->LoadedTrack->HotCues[h];
          if (hc.ID == (unsigned int)(p + 1)) {
            padVal = 0x7F; // Lit up HotCue Pad

            bool isApproaching = false;
            if (ds->CurrentBPM > 0) {
              uint32_t currentPosMs = ds->PositionMs;
              if (ds->IsPreviewing && globalAudioEngine && ds->ID >= 0 && ds->ID < 2) {
                DeckAudioState *audioState = &globalAudioEngine->Decks[ds->ID];
                if (audioState->SampleRate > 0) {
                  currentPosMs = (uint32_t)((audioState->Position / (double)audioState->SampleRate) * 1000.0);
                }
              }

              double distanceMs = (double)hc.Start - (double)currentPosMs;
              if (distanceMs > 0 && distanceMs <= (60000.0 / ds->CurrentBPM) * 16.0) {
                isApproaching = true;
              }
            }

            // Active Loop or Approaching blinking (matches bottomstrip.c 4 Hz flash rate)
            if ((hc.Status == 4 || isApproaching) && (int)(GetTime() * 4) % 2 == 0) {
              padVal = 0x00;
            }
            break;
          }
        }
      }

      // HotCue Mode Pad LED — midino = pad index 0..7
      if (forceSend || padVal != lastPadVals[i][p]) {
        MIDI_SendShortMsg(reg_pad[i].status, (uint8_t)p, padVal);
        lastPadVals[i][p] = padVal;
      }

      // Beat Loop Mode Pad LED — midino = beatloop base + pad index
      uint8_t beatLoopPadVal =
          (isLooping && p == 2) ? 0x7F : (padVal > 0 ? 0x20 : 0x00);
      if (forceSend || beatLoopPadVal != lastPadVals[i][p + 8]) {
        MIDI_SendShortMsg(reg_beatloop[i].status,
                          reg_beatloop[i].midino + (uint8_t)p, beatLoopPadVal);
        lastPadVals[i][p + 8] = beatLoopPadVal;
      }
    }

    // -------------------------------------------------------------
    // 4. JOG RING LEDs (Jog Wheel Ring Illumination & Position Indicator)
    // -------------------------------------------------------------
    bool hasTrack =
        (ds && ds->LoadedTrack) || (audio && audio->TotalSamples > 0);
    bool isTouch = (ds && ds->IsTouching) || (audio && audio->IsTouching);

    // Jog Outer/Inner Ring LED (0x9F, data1 = deck index 0..3)
    // IMPORTANT: 0x9F 0xNN 0x7F is the Pioneer DDJ hardware handshake byte —
    // NEVER send 0x7F here during normal playback or it re-triggers LED init.
    uint8_t jogRingVal = 0x00;
    if (hasTrack) {
      if (isTouch)
        jogRingVal = 0x50; // Touched: bright but not 0x7F
      else if (isPlaying)
        jogRingVal = 0x60; // Playing: bright
      else
        jogRingVal = 0x28; // Loaded/idle: dim
    }
    if (forceSend || jogRingVal != lastJogRingVal[i]) {
      MIDI_SendShortMsg(reg_jog_ring[i].status, reg_jog_ring[i].midino,
                        jogRingVal);
      lastJogRingVal[i] = jogRingVal;
    }

    // Jog Ring Position Rotation LED
    uint8_t jogPos72 = 1;
    uint8_t jogPos127 = 0;

    if (hasTrack && audio && audio->SampleRate > 0) {
      double posSec = (double)audio->Position / (double)audio->SampleRate;
      double rotAngle =
          fmod(posSec / 1.8, 1.0); // 1.8 sec per 33 1/3 RPM revolution
      if (rotAngle < 0)
        rotAngle += 1.0;
      jogPos72 =
          1 + (uint8_t)(rotAngle * 71.0f); // 1..72 range for Pioneer 0xBB ring
      if (jogPos72 > 72)
        jogPos72 = 72;
      jogPos127 = (uint8_t)(rotAngle * 127.0f);
    }

    if (forceSend || jogPos72 != lastJogPosVal[i]) {
      MIDI_SendShortMsg(reg_jog_pos[i].status, reg_jog_pos[i].midino, jogPos72);
      MIDI_SendShortMsg(reg_jog_cc[i].status, 0x2A, jogPos127);
      MIDI_SendShortMsg(reg_jog_cc[i].status, 0x2B, jogRingVal);
      lastJogPosVal[i] = jogPos72;
    }
  }

  // -------------------------------------------------------------
  // 5. BEAT FX & MASTER HEADPHONE CUE LEDs
  // -------------------------------------------------------------
  COType coT2;
  float *isFxOnPtr = (float *)CO_Find("[Master]", "beatfx_on", &coT2);
  bool isFxOn = isFxOnPtr ? (*isFxOnPtr > 0.0f) : (engine ? engine->BeatFX.isFxOn : false);
  bool blinkState = (fmod(GetTime(), 0.5) < 0.25); // Exact UI blink rate matching
  uint8_t bfxButtonVal = isFxOn ? (blinkState ? 0x7F : 0x00) : 0x00;
  uint8_t bfxSelectVal = isFxOn ? (blinkState ? 0x7F : 0x20) : 0x20;

  float *releasePtr =
      (float *)CO_Find("[EffectRack1_EffectUnit1]", "release_fx", &coT2);
  bool isReleaseFx = releasePtr ? (*releasePtr > 0.0f) : false;
  uint8_t releaseFxVal = isReleaseFx ? 0x7F : 0x00;

  float *masterPflPtr = (float *)CO_Find("[Master]", "pfl", &coT2);
  bool isMasterPfl = masterPflPtr ? (*masterPflPtr > 0.0f) : false;
  uint8_t masterPflVal = isMasterPfl ? 0x7F : 0x00;

  if (forceSend || bfxButtonVal != lastBeatFxButtonVal) {
    MIDI_SendShortMsg(reg_beatfx_button.status, reg_beatfx_button.midino,
                      bfxButtonVal);
    lastBeatFxButtonVal = bfxButtonVal;
  }
  if (forceSend || bfxSelectVal != lastBeatFxSelectVal) {
    MIDI_SendShortMsg(reg_beatfx_select.status, reg_beatfx_select.midino,
                      bfxSelectVal);
    lastBeatFxSelectVal = bfxSelectVal;
  }
  if (forceSend || releaseFxVal != lastReleaseFxVal) {
    MIDI_SendShortMsg(reg_release_fx.status, reg_release_fx.midino,
                      releaseFxVal);
    lastReleaseFxVal = releaseFxVal;
  }
  if (forceSend || masterPflVal != lastMasterPflVal) {
    MIDI_SendShortMsg(reg_master_pfl.status, reg_master_pfl.midino,
                      masterPflVal);
    lastMasterPflVal = masterPflVal;
  }

  // Beat FX CH Selector LEDs (Ch 1..4 & Master)
  int targetCh = engine ? engine->BeatFX.targetChannel : 0;
  for (int c = 0; c < 5; c++) {
    uint8_t chVal = (targetCh == c) ? 0x7F : 0x00;
    if (forceSend || chVal != lastBeatFxChVal[c]) {
      MIDI_SendShortMsg(reg_beatfx_ch[c].status, reg_beatfx_ch[c].midino,
                        chVal);
      lastBeatFxChVal[c] = chVal;
    }
  }

  // Beat FX Beat Select LEDs (Beat Left / Right)
  uint8_t beatLeftVal = 0x20;
  uint8_t beatRightVal = 0x20;
  if (forceSend || beatLeftVal != lastBeatLeftVal) {
    MIDI_SendShortMsg(reg_beat_left.status, reg_beat_left.midino, beatLeftVal);
    lastBeatLeftVal = beatLeftVal;
  }
  if (forceSend || beatRightVal != lastBeatRightVal) {
    MIDI_SendShortMsg(reg_beat_right.status, reg_beat_right.midino,
                      beatRightVal);
    lastBeatRightVal = beatRightVal;
  }
}

void MIDI_ResetAllLEDs(void) {
  MIDI_ResetVuMeters();

  MIDI_SendShortMsg(reg_beatfx_button.status, reg_beatfx_button.midino, 0);
  MIDI_SendShortMsg(reg_beatfx_select.status, reg_beatfx_select.midino, 0);
  MIDI_SendShortMsg(reg_release_fx.status, reg_release_fx.midino, 0);
  MIDI_SendShortMsg(reg_master_pfl.status, reg_master_pfl.midino, 0);
  MIDI_SendShortMsg(reg_beat_left.status, reg_beat_left.midino, 0);
  MIDI_SendShortMsg(reg_beat_right.status, reg_beat_right.midino, 0);

  for (int c = 0; c < 5; c++) {
    MIDI_SendShortMsg(reg_beatfx_ch[c].status, reg_beatfx_ch[c].midino, 0);
    lastBeatFxChVal[c] = 0;
  }

  lastBeatFxButtonVal = 0;
  lastBeatFxSelectVal = 0;
  lastReleaseFxVal = 0;
  lastMasterPflVal = 0;
  lastBeatLeftVal = 0;
  lastBeatRightVal = 0;

  for (int i = 0; i < 4; i++) {
    MIDI_SendShortMsg(reg_loop_in[i].status, reg_loop_in[i].midino, 0);
    MIDI_SendShortMsg(reg_loop_out[i].status, reg_loop_out[i].midino, 0);
    MIDI_SendShortMsg(reg_reloop[i].status, reg_reloop[i].midino, 0);
    MIDI_SendShortMsg(reg_play[i].status, reg_play[i].midino, 0);
    MIDI_SendShortMsg(reg_cue[i].status, reg_cue[i].midino, 0);
    MIDI_SendShortMsg(reg_sync[i].status, reg_sync[i].midino, 0);
    MIDI_SendShortMsg(reg_sync_leader[i].status, reg_sync_leader[i].midino, 0);
    MIDI_SendShortMsg(reg_master_tempo[i].status, reg_master_tempo[i].midino,
                      0);
    MIDI_SendShortMsg(reg_slip[i].status, reg_slip[i].midino, 0);
    MIDI_SendShortMsg(reg_keylock[i].status, reg_keylock[i].midino, 0);
    MIDI_SendShortMsg(reg_quantize[i].status, reg_quantize[i].midino, 0);
    MIDI_SendShortMsg(reg_pfl[i].status, reg_pfl[i].midino, 0);
    MIDI_SendShortMsg(reg_hotcue_mode[i].status, reg_hotcue_mode[i].midino, 0);
    MIDI_SendShortMsg(reg_beatloop[i].status, reg_beatloop_mode[i].midino, 0);
    MIDI_SendShortMsg(reg_beatjump_mode[i].status, reg_beatjump_mode[i].midino,
                      0);
    MIDI_SendShortMsg(reg_sampler_mode[i].status, reg_sampler_mode[i].midino,
                      0);
    MIDI_SendShortMsg(reg_padfx1_mode[i].status, reg_padfx1_mode[i].midino, 0);
    MIDI_SendShortMsg(reg_padfx2_mode[i].status, reg_padfx2_mode[i].midino, 0);
    MIDI_SendShortMsg(reg_keyboard_mode[i].status, reg_keyboard_mode[i].midino,
                      0);
    MIDI_SendShortMsg(reg_keyshift_mode[i].status, reg_keyshift_mode[i].midino,
                      0);
    MIDI_SendShortMsg(reg_jog_ring[i].status, reg_jog_ring[i].midino, 0);
    MIDI_SendShortMsg(reg_jog_pos[i].status, reg_jog_pos[i].midino, 0);
    MIDI_SendShortMsg(reg_jog_cc[i].status, 0x2A, 0);
    MIDI_SendShortMsg(reg_jog_cc[i].status, 0x2B, 0);

    lastLoopInVal[i] = 0;
    lastLoopOutVal[i] = 0;
    lastReloopVal[i] = 0;
    lastPlayVal[i] = 0;
    lastCueVal[i] = 0;
    lastSyncVal[i] = 0;
    lastSyncLeaderVal[i] = 0;
    lastMasterTempoVal[i] = 0;
    lastSlipVal[i] = 0;
    lastKeylockVal[i] = 0;
    lastQuantizeVal[i] = 0;
    lastPflVal[i] = 0;
    lastHotcueModeVal[i] = 0;
    lastBeatloopModeVal[i] = 0;
    lastBeatjumpModeVal[i] = 0;
    lastSamplerModeVal[i] = 0;
    lastPadfx1ModeVal[i] = 0;
    lastPadfx2ModeVal[i] = 0;
    lastKeyboardModeVal[i] = 0;
    lastKeyshiftModeVal[i] = 0;
    lastJogRingVal[i] = 0;
    lastJogPosVal[i] = 0;

    for (int p = 0; p < 8; p++) {
      MIDI_SendShortMsg(reg_pad[i].status, (uint8_t)p, 0);
      MIDI_SendShortMsg(reg_beatloop[i].status,
                        reg_beatloop[i].midino + (uint8_t)p, 0);
      lastPadVals[i][p] = 0;
      lastPadVals[i][p + 8] = 0;
    }
  }
}

void MIDI_ExecuteScript(MidiMapping *map, int actionId, uint8_t status,
                        uint8_t midino, uint8_t value) {
  (void)midino;
  int deck = (status & 0x0F) % 4;
  const char *groupNames[4] = {"[Channel1]", "[Channel2]", "[Channel3]",
                               "[Channel4]"};
  const char *group = groupNames[deck];
  int targetDeckIdx = deck % 2; // 0 for Deck A (Ch 1/3), 1 for Deck B (Ch 2/4)

  switch(actionId) {
  case SCRIPT_ACTION_SHIFT: {
    map->modifiers[0] = (value > 0); // Modifier 0 is Shift
    CO_SetValue(group, "shift", (value > 0) ? 1.0f : 0.0f);
    } break;
  case SCRIPT_ACTION_JOG_TURN:
  case SCRIPT_ACTION_JOG_SEARCH: {
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
          (actionId == SCRIPT_ACTION_JOG_SEARCH) || map->modifiers[0];
      float scale = isSearch ? 2.0f : (touching ? 0.1f : 0.005f);
      CO_AddValue(group, "jog", delta * scale);
    }
    } break;
  case SCRIPT_ACTION_JOG_TOUCH: {
    bool touching = (value > 0);
    CO_SetValue(group, "touch", touching ? 1.0f : 0.001f);
    if (globalAudioEngine && targetDeckIdx >= 0 && targetDeckIdx < 2) {
      DeckAudio_SetJogTouch(&globalAudioEngine->Decks[targetDeckIdx], touching);
    }
    } break;
  case SCRIPT_ACTION_BEAT_TAP: {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_tap", 1.0f);
    } break;
  case SCRIPT_ACTION_BEATFX_NEXT: {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_next", 1.0f);
    } break;
  case SCRIPT_ACTION_BEATFX_PREV: {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_prev", 1.0f);
    } break;
  case SCRIPT_ACTION_BEATFX_DEPTH: {
    float depth = (float)value / 127.0f;
    CO_SetValue("[Master]", "beatfx_drywet", depth);
  } break;
  case SCRIPT_ACTION_BEATFX_TOGGLE: {
    if (value > 0)
      CO_SetValue("[Master]", "beatfx_toggle", 1.0f);
    } break;
  case SCRIPT_ACTION_PAD_MODE: {
    if (value > 0) {
      if (1 /*hotcue*/ ||
          midino == 0x1B)
        CO_SetValue(group, "padmode", 0.0f); // PAD_MODE_HOT_CUE (0)
      else if (1 /*beatloop*/ || midino == 0x6D)
        CO_SetValue(group, "padmode", 1.0f); // PAD_MODE_BEAT_LOOP (1)
      else if (1 /*padfx*/ || midino == 0x1E || midino == 0x6B)
        CO_SetValue(group, "padmode", 2.0f); // PAD_MODE_SLIP_LOOP (2)
      else if (1 /*beatjump*/ || midino == 0x20)
        CO_SetValue(group, "padmode", 3.0f); // PAD_MODE_BEAT_JUMP (3)
      else if (1 /*sampler*/ || midino == 0x22)
        CO_SetValue(group, "padmode", 4.0f); // PAD_MODE_GATE_CUE (4)
      else if (1 /*releasefx*/ || midino == 0x69 ||
               midino == 0x6F)
        CO_SetValue(group, "padmode", 5.0f); // PAD_MODE_RELEASE_FX (5)
    }
    } break;
  case SCRIPT_ACTION_SAMPLER_PAD: {
    if (value > 0) {
      CO_SetValue("[Library]", (targetDeckIdx == 0) ? "loadA" : "loadB", 1.0f);
    }
    } break;
  case SCRIPT_ACTION_LOOP_IN_ADJUST: {
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
    } break;
  case SCRIPT_ACTION_LOOP_OUT_ADJUST: {
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
    } break;
  case SCRIPT_ACTION_CUE_LOOP_LEFT: {
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
    } break;
  case SCRIPT_ACTION_CUE_LOOP_RIGHT: {
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
    } break;
  case SCRIPT_ACTION_TEMPO_MSB: {
    if (deck >= 0 && deck < 4) {
      highResMSB[deck] = value;
    }
    } break;
  case SCRIPT_ACTION_TEMPO_LSB: {
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
    } break;
  case SCRIPT_ACTION_TEMPO_RANGE: {
    COType t;
    int *rangePtr = (int *)CO_Find(group, "tempo_range", &t);
    if (rangePtr) {
      *rangePtr = (*rangePtr + 1) % 4;
    }
    } break;
  case SCRIPT_ACTION_SYNC: {
    if (value > 0)
      CO_SetValue(group, "sync", 1.0f);
    } break;
  case SCRIPT_ACTION_QUANTIZE: {
    if (value > 0)
      CO_ToggleValue(group, "quantize");
    } break;
  case SCRIPT_ACTION_SLIP: {
    if (value > 0)
      CO_ToggleValue(group, "slip");
    } break;
  case SCRIPT_ACTION_MERGE_FX_TURN: {
    float delta = (value >= 64) ? (float)(value - 128) : (float)value;
    CO_AddValue("[Master]", "beatfx_drywet", delta * 0.02f);
    } break;
  case SCRIPT_ACTION_MERGE_FX_PRESS: {
    if (value > 0)
      CO_ToggleValue("[Master]", "beatfx_on");
    } break;
  case SCRIPT_ACTION_LOAD_TRACK: {
    CO_SetValue("[Library]", (targetDeckIdx == 0) ? "loadA" : "loadB", 1.0f);
    } break;
  case SCRIPT_ACTION_BROWSE_CLICK: {
    if (value > 0)
      CO_SetValue("[Library]", "enter", 1.0f);
    } break;
  case SCRIPT_ACTION_BROWSE_TOGGLE: {
    if (value > 0)
      CO_SetValue("[Library]", "browser_toggle", 1.0f);
    } break;
  case SCRIPT_ACTION_HEAD_MIX: {
    float normVal = (float)value / 127.0f;
    CO_SetValue("[Master]", "headphone_mix", normVal);
    CO_SetValue("[Master]", "headMix", normVal);
    } break;
  case SCRIPT_ACTION_BEATJUMP_PAD: {
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
    } break;
  case SCRIPT_ACTION_BEATJUMP_DEC: {
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
    } break;
  case SCRIPT_ACTION_BEATJUMP_INC: {
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
    } break;
  case SCRIPT_ACTION_DECK_CONTROL_L: {
    if (value > 0) {
      CO_ToggleValue("[Channel1]", "deck_layer");
    }
    } break;
  case SCRIPT_ACTION_DECK_CONTROL_R: {
    if (value > 0) {
      CO_ToggleValue("[Channel2]", "deck_layer");
    }
    } break;
  case SCRIPT_ACTION_KEYBOARD_BTN: {
    if (value > 0) {
      int semitone = 0;
      if (midino >= 0x70 && midino <= 0x77) {
        semitone = (int)midino - 0x74; // 0x74 is 0 semitones (reset pitch)
      } else if (midino >= 0x40 && midino <= 0x47) {
        semitone = (int)midino - 0x44;
      }
      CO_SetValue(group, "key_shift", (float)semitone);
    }
    } break;
  case SCRIPT_ACTION_BROWSE_SCROLL: {
    float diff = (value >= 64) ? (float)(value - 128) : (float)value;
    CO_AddValue("[Library]", "scroll", diff);
  } break;
  default: break;
  }
}
