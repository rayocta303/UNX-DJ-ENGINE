#include "logic/quantize.h"
#include <stdlib.h>
#include <math.h>

uint32_t Quantize_GetNearestBeatMs(TrackState *track, uint32_t currentMs) {
    if (!track || track->BeatGridCount == 0) return currentMs;
    
    // Find closest beat in BeatGrid array
    uint32_t closestBeatMs = track->BeatGrid[0];
    int minDiff = abs((int)currentMs - (int)closestBeatMs);
    
    for (int i = 1; i < track->BeatGridCount; i++) {
        int diff = abs((int)currentMs - (int)track->BeatGrid[i]);
        if (diff < minDiff) {
            minDiff = diff;
            closestBeatMs = track->BeatGrid[i];
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

float Quantize_GetBeatFXLengthMs(TrackState *track, float targetRatio) {
    if (!track || track->BeatGridCount < 2) return 0.0f;
    
    // Calculate average ms per beat from the grid
    int count = track->BeatGridCount;
    float avgBeatLength = (float)(track->BeatGrid[count - 1] - track->BeatGrid[0]) / (float)(count - 1);
    
    // Fallback if there is an error
    if (avgBeatLength <= 0.0f) return 0.0f;
    
    return avgBeatLength * targetRatio;
}
