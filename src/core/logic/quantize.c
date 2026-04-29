#include "core/logic/quantize.h"
#include <stdlib.h>
#include <math.h>

int64_t Quantize_GetNearestBeatMs(TrackState *track, int64_t currentMs) {
    if (!track || track->BeatGridCount == 0) return currentMs;
    
    // Find closest beat in BeatGrid array
    int64_t closestBeatMs = (int64_t)track->BeatGrid[0].Time;
    int64_t minDiff = llabs(currentMs - closestBeatMs);
    
    for (int i = 1; i < track->BeatGridCount; i++) {
        int64_t diff = llabs(currentMs - (int64_t)track->BeatGrid[i].Time);
        if (diff < minDiff) {
            minDiff = diff;
            closestBeatMs = (int64_t)track->BeatGrid[i].Time;
        } else if (diff > minDiff) {
            // Since BeatGrid is sorted, distance starts increasing after we pass the minimum
            break; 
        }
    }
    
    return closestBeatMs;
}

int32_t Quantize_GetPhaseErrorMs(TrackState *track, int64_t currentMs) {
    if (!track || track->BeatGridCount == 0) return 0;
    int64_t nearest = Quantize_GetNearestBeatMs(track, currentMs);
    return (int32_t)(currentMs - nearest);
}

int32_t Quantize_GetWaitMs(TrackState *track, int64_t currentMs) {
    if (!track || track->BeatGridCount == 0) return 0;
    
    // Look forward for the *next* or *current* beat grid marker
    for (int i = 0; i < track->BeatGridCount; i++) {
        if ((int64_t)track->BeatGrid[i].Time >= currentMs) {
            return (int32_t)((int64_t)track->BeatGrid[i].Time - currentMs);
        }
    }
    return 0; // If past end of grids, don't wait
}

double Quantize_GetBeatDistance(TrackState *track, int64_t currentMs) {
    if (!track || track->BeatGridCount < 2) return 0.0;

    for (int i = 0; i < track->BeatGridCount - 1; i++) {
        if (currentMs >= (int64_t)track->BeatGrid[i].Time && currentMs < (int64_t)track->BeatGrid[i+1].Time) {
            int64_t beatStart = (int64_t)track->BeatGrid[i].Time;
            int64_t beatEnd = (int64_t)track->BeatGrid[i+1].Time;
            int64_t beatLen = beatEnd - beatStart;
            if (beatLen == 0) return 0.0;
            return (double)(currentMs - beatStart) / (double)beatLen;
        }
    }
    return 0.0;
}

int Quantize_GetCurrentBeat(TrackState *track, int64_t currentMs) {
    if (!track || track->BeatGridCount == 0) return 1;
    for (int i = 0; i < track->BeatGridCount; i++) {
        if ((int64_t)track->BeatGrid[i].Time > currentMs) {
            if (i == 0) return track->BeatGrid[0].BeatNumber;
            return track->BeatGrid[i - 1].BeatNumber;
        }
    }
    return track->BeatGrid[track->BeatGridCount - 1].BeatNumber;
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
