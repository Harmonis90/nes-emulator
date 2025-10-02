#pragma once

// Combine channels using the NES’ nonlinear mixer approximation later.
// For now this just averages with light weighting so you can hear something.
float apu_mixer_mix(float p1, float p2, float tri, float noi, float dmc);
