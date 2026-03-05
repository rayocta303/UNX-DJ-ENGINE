#include "ui/player/player.h"
#include "ui/components/theme.h"
#include "ui/components/fonts.h"
#include "ui/components/helpers.h"

static int Player_Update(Component *base) {
    PlayerRenderer *p = (PlayerRenderer *)base;
    p->WaveA.base.Update((Component*)&p->WaveA);
    p->WaveB.base.Update((Component*)&p->WaveB);
    p->InfoA.base.Update((Component*)&p->InfoA);
    p->InfoB.base.Update((Component*)&p->InfoB);
    p->BeatFX.base.Update((Component*)&p->BeatFX);
    p->FXBar.base.Update((Component*)&p->FXBar);
    p->StripA.base.Update((Component*)&p->StripA);
    p->StripB.base.Update((Component*)&p->StripB);
    return 0;
}

static void Player_Draw(Component *base) {
    PlayerRenderer *p = (PlayerRenderer *)base;

    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, ColorBlack);

    p->InfoA.base.Draw((Component*)&p->InfoA);
    p->InfoB.base.Draw((Component*)&p->InfoB);
    
    p->WaveA.base.Draw((Component*)&p->WaveA);
    p->WaveB.base.Draw((Component*)&p->WaveB);
    
    p->BeatFX.base.Draw((Component*)&p->BeatFX);
    p->FXBar.base.Draw((Component*)&p->FXBar);
    
    p->StripA.base.Draw((Component*)&p->StripA);
    p->StripB.base.Draw((Component*)&p->StripB);
}

void PlayerRenderer_Init(PlayerRenderer *r, DeckState *a, DeckState *b, BeatFXState *fx, AudioEngine *audioPlugin) {
    r->base.Update = Player_Update;
    r->base.Draw = Player_Draw;
    r->DeckA = a;
    r->DeckB = b;
    r->FXState = fx;
    r->AudioPlugin = audioPlugin;

    DeckInfoPanel_Init(&r->InfoA, 0, a);
    DeckInfoPanel_Init(&r->InfoB, 1, b);
    
    DeckStrip_Init(&r->StripA, 0, a);
    DeckStrip_Init(&r->StripB, 1, b);
    
    WaveformRenderer_Init(&r->WaveA, 0, a);
    WaveformRenderer_Init(&r->WaveB, 1, b);
    
    BeatFXPanel_Init(&r->BeatFX, fx);
    BeatFXSelectBar_Init(&r->FXBar, fx, a, b);
}
