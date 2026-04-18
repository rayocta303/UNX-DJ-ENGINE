#include "ui/player/player.h"
#include <stddef.h>
#include <stdlib.h>
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"
#include "ui/components/theme.h"
#include "ui/components/assets_bundle.h"


static int Player_Update(Component *base) {
  PlayerRenderer *p = (PlayerRenderer *)base;
  p->WaveA.base.Update((Component *)&p->WaveA);
  p->WaveB.base.Update((Component *)&p->WaveB);
  p->InfoA.base.Update((Component *)&p->InfoA);
  p->InfoB.base.Update((Component *)&p->InfoB);
  p->BeatFX.base.Update((Component *)&p->BeatFX);
  p->FXBar.base.Update((Component *)&p->FXBar);

  // Sync BeatFX state to AudioEngine
  float masterBpm = 120.0f;
  if (p->DeckA->IsMaster)
    masterBpm = p->DeckA->CurrentBPM;
  else if (p->DeckB->IsMaster)
    masterBpm = p->DeckB->CurrentBPM;
  else
    masterBpm = p->DeckA->CurrentBPM;

  if (masterBpm < 1.0f)
    masterBpm = 120.0f;

  static const float XPadRatios[] = {0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
  int padIdx = p->FXState->SelectedPad;
  if (padIdx < 0)
    padIdx = 4;
  if (padIdx >= 6)
    padIdx = 5;

  float ratio = XPadRatios[padIdx];
  float fxMs = (60000.0f / masterBpm) * ratio;

  p->AudioPlugin->BeatFX.activeFX = p->FXState->SelectedFX;
  p->AudioPlugin->BeatFX.targetChannel = p->FXState->SelectedChannel;
  p->AudioPlugin->BeatFX.isFxOn = p->FXState->IsFXOn;
  p->AudioPlugin->BeatFX.beatMs = fxMs;
  p->AudioPlugin->BeatFX.levelDepth = 0.5f; // Hardcoded default for now
  p->AudioPlugin->BeatFX.scrubVal = p->FXState->XPadScrubValue;
  p->AudioPlugin->BeatFX.isScrubbing = p->FXState->IsXPadScrubbing;

  return 0;
}

static void Player_Draw(Component *base) {
  PlayerRenderer *p = (PlayerRenderer *)base;

  DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

#if !defined(PLATFORM_IOS)
  // Draw shared background logo behind waveforms (disabled on iOS for VRAM safety)
  if (p->Logo.id != 0) {
    float waveAreaY = TOP_BAR_H;
    float waveAreaH = SCREEN_HEIGHT - TOP_BAR_H - FX_BAR_H - DECK_STR_H;
    float waveAreaW = SCREEN_WIDTH - SIDE_PANEL_W * 2.0f;
    float waveAreaX = SIDE_PANEL_W;

    float logoH = waveAreaH * 0.5f;
    float logoScale = logoH / p->Logo.height;
    float lw = p->Logo.width * logoScale;
    float lh = p->Logo.height * logoScale;

    if (lw > waveAreaW * 0.8f) {
      logoScale = (waveAreaW * 0.8f) / p->Logo.width;
      lw = p->Logo.width * logoScale;
      lh = p->Logo.height * logoScale;
    }

    float logoOpacity = 0.3f;
    if (p->DeckA->LoadedTrack != NULL || p->DeckB->LoadedTrack != NULL) {
      logoOpacity = 0.1f;
    }

    DrawTextureEx(p->Logo, (Vector2){waveAreaX + (waveAreaW - lw) / 2.0f, waveAreaY + (waveAreaH - lh) / 2.0f}, 0.0f, logoScale, Fade(WHITE, logoOpacity));
  }
#endif

  p->InfoA.base.Draw((Component *)&p->InfoA);
  p->InfoB.base.Draw((Component *)&p->InfoB);

  p->WaveA.base.Draw((Component *)&p->WaveA);
  p->WaveB.base.Draw((Component *)&p->WaveB);

  p->BeatFX.base.Draw((Component *)&p->BeatFX);
  p->FXBar.base.Draw((Component *)&p->FXBar);
}

void PlayerRenderer_Init(PlayerRenderer *r, DeckState *a, DeckState *b,
                         BeatFXState *fx, AudioEngine *audioPlugin) {
  r->base.Update = Player_Update;
  r->base.Draw = Player_Draw;
  r->DeckA = a;
  r->DeckB = b;
  r->FXState = fx;
  r->AudioPlugin = audioPlugin;

  DeckInfoPanel_Init(&r->InfoA, 0, a);
  DeckInfoPanel_Init(&r->InfoB, 1, b);

  WaveformRenderer_Init(&r->WaveA, 0, a, b);
  WaveformRenderer_Init(&r->WaveB, 1, b, a);

  BeatFXPanel_Init(&r->BeatFX, fx, a, b);
  BeatFXSelectBar_Init(&r->FXBar, fx, a, b, r->AudioPlugin);

#if !defined(PLATFORM_IOS)
  // Load shared logo from memory bundle
  Image img = LoadImageFromMemory(".png", unx_logo, unx_logo_size);
  if (img.data != NULL) {
    if (img.width > 1080) {
        float aspect = (float)img.height / (float)img.width;
        ImageResize(&img, 1080, (int)(1080.0f * aspect));
    }
    r->Logo = LoadTextureFromImage(img);
    GenTextureMipmaps(&r->Logo);
    SetTextureFilter(r->Logo, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
  } else {
    r->Logo = (Texture2D){0};
  }
#else
  r->Logo = (Texture2D){0};
#endif
}
