#include "audio/apu_mixer.h"

float apu_mixer_mix(float p1, float p2, float tri, float noi, float dmc) {
    // placeholder weighting; replace with NES nonlinear mixer later
    const float pulse = (p1 + p2) * 0.25f;
    const float tonal = tri * 0.35f;
    const float noise = noi * 0.20f;
    const float d     = dmc * 0.20f;
    float s = pulse + tonal + noise + d;

    // simple clamp
    if (s > 1.0f) s = 1.0f;
    if (s < -1.0f) s = -1.0f;
    return s;
}
