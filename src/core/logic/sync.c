#include "core/logic/sync.h"
#include "core/logic/quantize.h"
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
    
    // Scratch Safety: If either deck is being scratched, downgrade Beat Sync to BPM Sync
    // This prevents the synced deck from snapping or following the scratch movement.
    if (follower->SyncMode == 2 && (master->IsTouching || follower->IsTouching)) {
        follower->SyncMode = 1;
    }

    // Reset sync if sync is off or invalid
    if (follower->SyncMode == 0) return;

    // Both tracks must have valid BPM > 0
    if (master->OriginalBPM <= 0.0f || follower->OriginalBPM <= 0.0f) return;
    
    // 1. BPM Sync (Tempo Match)
    // We match follower's CurrentBPM to the Master's CurrentBPM using Engine's multiplier logic.
    double multiplier = Sync_DetermineBpmMultiplier(follower->OriginalBPM, master->CurrentBPM);
    float targetTempoPercent = ((master->CurrentBPM * multiplier / follower->OriginalBPM) - 1.0f) * 100.0f;
    
    // 2. Phase Sync (Beat Snap)
    if (follower->SyncMode == 2 && follower->IsPlaying && !follower->IsTouching && 
        follower->LoadedTrack && master->LoadedTrack) {
        
        double masterDist = Quantize_GetBeatDistance(master->LoadedTrack, master->PositionMs);
        double followerDist = Quantize_GetBeatDistance(follower->LoadedTrack, follower->PositionMs);
        
        // Shortest error in beat fraction (-0.5 to 0.5)
        double error = Sync_ShortestPercentageChange(masterDist, followerDist);
        
        // Calculate drift in milliseconds
        // We need the current beat length to convert fraction to ms
        uint32_t fCurrentMs = follower->PositionMs;
        uint32_t fBeatLen = 500; // Default fallback (120 BPM)
        for (int i = 0; i < follower->LoadedTrack->Analysis.BeatGridCount - 1; i++) {
            if (fCurrentMs >= follower->LoadedTrack->Analysis.BeatGrid[i].Time && 
                fCurrentMs < follower->LoadedTrack->Analysis.BeatGrid[i+1].Time) {
                fBeatLen = follower->LoadedTrack->Analysis.BeatGrid[i+1].Time - follower->LoadedTrack->Analysis.BeatGrid[i].Time;
                break;
            }
        }
        
        double driftMs = error * (double)fBeatLen;
        
        // Tracking drift status for UI feedback
        if (fabs(driftMs) > 5.0) follower->IsPhaseDrifted = true;
        else if (fabs(driftMs) < 1.0) follower->IsPhaseDrifted = false;

        // --- PRO SYNC LOGIC: Pitch Bending vs Snapping ---
        if (follower->IsTouching) {
            // Manual nudge: Disable automatic correction but track status
            follower->LastPhaseAdjustment = 0.0f;
        }
        // 1. Hard Snap: If error is huge (> 100ms) or we just started playing
        else if (fabs(driftMs) > 100.0) {
            Sync_RequestPhaseSnap(follower, master, audioEngine);
            follower->LastPhaseAdjustment = 0.0f;
            follower->IsPhaseDrifted = false;
        } 
        // 2. Smooth Correction: Pitch Bending for smaller errors
        else if (fabs(driftMs) > 1.0) {
            // Apply a pitch bend to catch up. 
            float correction = (float)(-driftMs / (fBeatLen * 2.0)); 
            
            // Cap the correction to 1.5% to avoid audible warbling
            if (correction > 0.015f) correction = 0.015f;
            if (correction < -0.015f) correction = -0.015f;
            
            follower->LastPhaseAdjustment = correction;
        } else {
            // Perfectly in sync
            follower->LastPhaseAdjustment = 0.0f;
            follower->IsPhaseDrifted = false;
        }
    } else {
        follower->LastPhaseAdjustment = 0.0f;
        follower->IsPhaseDrifted = false;
    }
    
    float finalTempoPercent = targetTempoPercent;

    // Soft takeover logic
    if (fabsf(follower->TempoPercent - finalTempoPercent) > 0.01f) {
        follower->TargetTakeoverPercent = finalTempoPercent;
        follower->PitchTakeoverActive = true;
    }
    follower->TempoPercent = finalTempoPercent;
}

void Sync_RequestPhaseSnap(DeckState *follower, DeckState *master, AudioEngine *audioEngine) {
    if (!follower || !master || !audioEngine || !follower->LoadedTrack || !master->LoadedTrack) return;
    
    // Safety check: Never snap the master deck!
    if (follower->IsMaster || !master->IsMaster) return;
    
    // 1. Get current beat distance of the Master
    double masterDist = Quantize_GetBeatDistance(master->LoadedTrack, master->PositionMs);
    
    // 2. Find where the follower *should* be to match that distance in its current beat
    // We look at the follower's current beat markers
    TrackState *fTrack = follower->LoadedTrack;
    uint32_t currentMs = follower->PositionMs;
    
    uint32_t beatStart = 0;
    uint32_t beatEnd = 0;
    
    // Find the beat segment the follower is currently in
    for (int i = 0; i < fTrack->Analysis.BeatGridCount - 1; i++) {
        if (currentMs >= fTrack->Analysis.BeatGrid[i].Time && currentMs < fTrack->Analysis.BeatGrid[i+1].Time) {
            beatStart = fTrack->Analysis.BeatGrid[i].Time;
            beatEnd = fTrack->Analysis.BeatGrid[i+1].Time;
            break;
        }
    }
    
    // Fallback if not found (e.g. at the very start or end)
    if (beatEnd == 0 && fTrack->Analysis.BeatGridCount >= 2) {
        beatStart = fTrack->Analysis.BeatGrid[0].Time;
        beatEnd = fTrack->Analysis.BeatGrid[1].Time;
    }
    
    if (beatEnd > beatStart) {
        uint32_t beatLen = beatEnd - beatStart;
        uint32_t targetMs = beatStart + (uint32_t)(masterDist * (double)beatLen);
        
        // 3. Perform the jump in the audio engine
        int deckIdx = (follower->ID == 0) ? 0 : 1; 
        DeckAudioState *audio = &audioEngine->Decks[deckIdx];

        // If looping, ensure targetMs wraps within the loop boundaries
        if (audio->IsLooping) {
            double sr = (double)audio->SampleRate;
            uint32_t loopStartMs = (uint32_t)((audio->LoopStartPos / sr) * 1000.0);
            uint32_t loopEndMs = (uint32_t)((audio->LoopEndPos / sr) * 1000.0);
            uint32_t loopLenMs = loopEndMs - loopStartMs;

            if (loopLenMs > 0) {
                while (targetMs >= loopEndMs) targetMs -= loopLenMs;
                while (targetMs < loopStartMs) targetMs += loopLenMs;
            }
        }

        DeckAudio_JumpToMs(audio, targetMs);
    }
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
