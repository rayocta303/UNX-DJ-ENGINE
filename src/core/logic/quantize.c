#include "core/logic/quantize.h"
#include <stdlib.h>
#include <math.h>

uint32_t Quantize_GetNearestBeatMs(TrackState *track, uint32_t currentMs) {
    if (!track || track->BeatGridCount == 0) return currentMs;
    
    // Find closest beat in BeatGrid array
    uint32_t closestBeatMs = track->BeatGrid[0].Time;
    int minDiff = abs((int)currentMs - (int)closestBeatMs);
    
    for (int i = 1; i < track->BeatGridCount; i++) {
        int diff = abs((int)currentMs - (int)track->BeatGrid[i].Time);
        if (diff < minDiff) {
            minDiff = diff;
            closestBeatMs = track->BeatGrid[i].Time;
        } else if (diff > minDiff) {
            // Since BeatGrid is sorted, distance starts increasing after we pass the minimum
            break; 
        }
    }
    
    return closestBeatMs;
}

int32_t Quantize_GetPhaseErrorMs(TrackState *track, uint32_t currentMs) {
    if (!track || track->BeatGridCount == 0) return 0;
    uint32_t nearest = Quantize_GetNearestBeatMs(track, currentMs);
    return (int32_t)currentMs - (int32_t)nearest;
}

int32_t Quantize_GetWaitMs(TrackState *track, uint32_t currentMs) {
    if (!track || track->BeatGridCount == 0) return 0;
    
    // Look forward for the *next* or *current* beat grid marker
    for (int i = 0; i < track->BeatGridCount; i++) {
        if (track->BeatGrid[i].Time >= currentMs) {
            return (int32_t)(track->BeatGrid[i].Time) - (int32_t)currentMs;
        }
    }
    return 0; // If past end of grids, don't wait
}

double Quantize_GetBeatDistance(TrackState *track, uint32_t currentMs) {
    if (!track || track->BeatGridCount < 2) return 0.0;

    for (int i = 0; i < track->BeatGridCount - 1; i++) {
        if (currentMs >= track->BeatGrid[i].Time && currentMs < track->BeatGrid[i+1].Time) {
            uint32_t beatStart = track->BeatGrid[i].Time;
            uint32_t beatEnd = track->BeatGrid[i+1].Time;
            uint32_t beatLen = beatEnd - beatStart;
            if (beatLen == 0) return 0.0;
            return (double)(currentMs - beatStart) / (double)beatLen;
        }
    }
    return 0.0;
}

float Quantize_GetBeatFXLengthMs(TrackState *track, float targetRatio) {
    if (!track || track->BeatGridCount < 2) return 0.0f;
    
    // Calculate average ms per beat from the grid
    int count = track->BeatGridCount;
    float avgBeatLength = (float)(track->BeatGrid[count - 1].Time - track->BeatGrid[0].Time) / (float)(count - 1);
    
    // Fallback if there is an error
    if (avgBeatLength <= 0.0f) return 0.0f;
    
    return avgBeatLength * targetRatio;
}
