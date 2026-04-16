#ifndef QUANTIZE_H
#define QUANTIZE_H

#include "ui/player/player_state.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns the timestamp (in ms) of the closest beat to the provided currentMs.
// Assumes track->BeatGrid is sorted ascending.
uint32_t Quantize_GetNearestBeatMs(TrackState *track, uint32_t currentMs);

// Returns (currentMs - NearestBeatMs).
// A negative value means the user hit the button early (before the beat).
// A positive value means the user hit the button late (after the beat).
int32_t Quantize_GetPhaseErrorMs(TrackState *track, uint32_t currentMs);

int32_t Quantize_GetWaitMs(TrackState *track, uint32_t currentMs);

// Returns the fractional distance between the current beat and the next (0.0 to 1.0)
double Quantize_GetBeatDistance(TrackState *track, uint32_t currentMs);

// Calculate the nearest exact fraction of a beat (1/8, 1/4, 1/2, 1, 2)
// This is useful for Beat FX (Delay/Echo) length calculations.
float Quantize_GetBeatFXLengthMs(TrackState *track, float targetRatio);

#ifdef __cplusplus
}
#endif

#endif // QUANTIZE_H
