#include "logic/sync.h"
#include "logic/quantize.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define kBpmUnity 1.0
#define kBpmHalve 0.5
#define kBpmDouble 2.0

double Sync_DetermineBpmMultiplier(double myBpm, double targetBpm) {
    if (myBpm <= 0.0 || targetBpm <= 0.0) return kBpmUnity;
    double unityRatio = myBpm / targetBpm;
    double unityRatioSquare = unityRatio * unityRatio;
    if (unityRatioSquare > kBpmDouble) return kBpmDouble;
    else if (unityRatioSquare < kBpmHalve) return kBpmHalve;
    return kBpmUnity;
}

double Sync_ShortestPercentageChange(double current_percentage, double target_percentage) {
    if (current_percentage == target_percentage) return 0.0;
    if (current_percentage < target_percentage) {
        double forwardDistance = target_percentage - current_percentage;
        double backwardsDistance = target_percentage - current_percentage - 1.0;
        return (fabs(forwardDistance) < fabs(backwardsDistance)) ? forwardDistance : backwardsDistance;
    } else {
        double forwardDistance = 1.0 - current_percentage + target_percentage;
        double backwardsDistance = target_percentage - current_percentage;
        return (fabs(forwardDistance) < fabs(backwardsDistance)) ? forwardDistance : backwardsDistance;
    }
}

void Sync_Update(DeckState *deckA, DeckState *deckB, AudioEngine *audioEngine) {
    (void)audioEngine;
    if (!deckA || !deckB) return;

    DeckState *master = NULL;
    DeckState *follower = NULL;

    if (deckA->IsMaster) {
        master = deckA;
        follower = deckB;
    } else if (deckB->IsMaster) {
        master = deckB;
        follower = deckA;
    }

    if (!master || !follower) return;
    
    // Reset sync if sync is off or invalid
    if (follower->SyncMode == 0) return;

    // Both tracks must have valid BPM > 0
    if (master->OriginalBPM <= 0.0f || follower->OriginalBPM <= 0.0f) return;
    
    // 1. BPM Sync (Tempo Match)
    // We match follower's CurrentBPM to the Master's CurrentBPM using Mixxx's multiplier logic.
    double multiplier = Sync_DetermineBpmMultiplier(follower->OriginalBPM, master->CurrentBPM);
    float targetTempoPercent = ((master->CurrentBPM * multiplier / follower->OriginalBPM) - 1.0f) * 100.0f;
    
    // Soft takeover logic
    if (fabsf(follower->TempoPercent - targetTempoPercent) > 0.01f) {
        follower->TargetTakeoverPercent = targetTempoPercent;
        follower->PitchTakeoverActive = true;
    }
    follower->TempoPercent = targetTempoPercent;
}

void DeckState_SetHardwarePitch(DeckState *deck, float hardwarePercent) {
    if (!deck) return;
    
    // Always track hardware position regardless
    deck->HardwarePitchPercent = hardwarePercent;
    
    if (deck->PitchTakeoverActive) {
        // We are locked. Did we cross over?
        // Easiest check: if the distance between current hardware value and target is very small.
        // Or if it crosses from above to below / below to above.
        // For simplicity, we unlock if hardware distance to target is less than 0.2%
        if (fabs(hardwarePercent - deck->TargetTakeoverPercent) < 0.2f) {
            deck->PitchTakeoverActive = false;
        }
    }
    
    // If not locked, the internal Tempo tracks the physical hardware
    if (!deck->PitchTakeoverActive) {
        deck->TempoPercent = hardwarePercent;
    }
}
