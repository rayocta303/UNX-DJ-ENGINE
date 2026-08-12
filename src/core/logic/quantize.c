#include "core/logic/quantize.h"
#include <stdlib.h>
#include <math.h>

int Quantize_GetDivisor(int resolution) {
    if (resolution == 0) return 8;
    if (resolution == 1) return 4;
    if (resolution == 2) return 2;
    return 1;
}

int64_t Quantize_GetNearestBeatMs(TrackState *track, int64_t currentMs, int divisor) {
    if (!track || track->Analysis.BeatGridCount == 0 || !track->Analysis.BeatGrid) return currentMs;
    if (divisor < 1) divisor = 1;
    
    int64_t closestMs = (int64_t)track->Analysis.BeatGrid[0].Time;
    int64_t minDiff = llabs(currentMs - closestMs);
    
    for (int i = 0; i < track->Analysis.BeatGridCount; i++) {
        int64_t beatStart = (int64_t)track->Analysis.BeatGrid[i].Time;
        int64_t beatEnd = beatStart;
        if (i < track->Analysis.BeatGridCount - 1) {
            beatEnd = (int64_t)track->Analysis.BeatGrid[i+1].Time;
        } else if (i > 0) {
            beatEnd = beatStart + (beatStart - (int64_t)track->Analysis.BeatGrid[i-1].Time);
        } else {
            beatEnd = beatStart + 500;
        }
        
        int64_t beatLen = beatEnd - beatStart;
        if (beatLen <= 0) continue;
        
        double segmentLen = (double)beatLen / (double)divisor;
        
        for (int j = 0; j < divisor; j++) {
            int64_t subBeatTime = beatStart + (int64_t)(j * segmentLen);
            int64_t diff = llabs(currentMs - subBeatTime);
            if (diff < minDiff) {
                minDiff = diff;
                closestMs = subBeatTime;
            }
        }
        if (beatStart > currentMs + beatLen) {
             break;
        }
    }
    
    return closestMs;
}

int32_t Quantize_GetPhaseErrorMs(TrackState *track, int64_t currentMs) {
    if (!track || track->Analysis.BeatGridCount == 0 || !track->Analysis.BeatGrid) return 0;
    int64_t nearest = Quantize_GetNearestBeatMs(track, currentMs, 1);
    return (int32_t)(currentMs - nearest);
}

int32_t Quantize_GetWaitMs(TrackState *track, int64_t currentMs) {
    if (!track || track->Analysis.BeatGridCount == 0 || !track->Analysis.BeatGrid) return 0;
    
    // Look forward for the *next* or *current* beat grid marker
    for (int i = 0; i < track->Analysis.BeatGridCount; i++) {
        if ((int64_t)track->Analysis.BeatGrid[i].Time >= currentMs) {
            return (int32_t)((int64_t)track->Analysis.BeatGrid[i].Time - currentMs);
        }
    }
    return 0; // If past end of grids, don't wait
}

double Quantize_GetBeatDistance(TrackState *track, int64_t currentMs) {
    if (!track || track->Analysis.BeatGridCount < 2 || !track->Analysis.BeatGrid) return 0.0;

    for (int i = 0; i < track->Analysis.BeatGridCount - 1; i++) {
        if (currentMs >= (int64_t)track->Analysis.BeatGrid[i].Time && currentMs < (int64_t)track->Analysis.BeatGrid[i+1].Time) {
            int64_t beatStart = (int64_t)track->Analysis.BeatGrid[i].Time;
            int64_t beatEnd = (int64_t)track->Analysis.BeatGrid[i+1].Time;
            int64_t beatLen = beatEnd - beatStart;
            if (beatLen == 0) return 0.0;
            return (double)(currentMs - beatStart) / (double)beatLen;
        }
    }
    return 0.0;
}

int Quantize_GetCurrentBeat(TrackState *track, int64_t currentMs) {
    if (!track || track->Analysis.BeatGridCount == 0 || !track->Analysis.BeatGrid) return 1;
    for (int i = 0; i < track->Analysis.BeatGridCount; i++) {
        if ((int64_t)track->Analysis.BeatGrid[i].Time > currentMs) {
            if (i == 0) return track->Analysis.BeatGrid[0].BeatNumber;
            return track->Analysis.BeatGrid[i - 1].BeatNumber;
        }
    }
    return track->Analysis.BeatGrid[track->Analysis.BeatGridCount - 1].BeatNumber;
}

float Quantize_GetBeatFXLengthMs(TrackState *track, float targetRatio) {
    if (!track || track->Analysis.BeatGridCount < 2 || !track->Analysis.BeatGrid) return 0.0f;
    
    // Calculate average ms per beat from the grid
    int count = track->Analysis.BeatGridCount;
    if (count < 2) return 0.0f;
    float avgBeatLength = (float)(track->Analysis.BeatGrid[count - 1].Time - track->Analysis.BeatGrid[0].Time) / (float)(count - 1);
    
    // Fallback if there is an error
    if (avgBeatLength <= 0.0f) return 0.0f;
    
    return avgBeatLength * targetRatio;
}
