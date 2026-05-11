#pragma once

#include <math.h>

#include "SpatialPos.h"

namespace qbm {

struct QuadGains {
    float fl;
    float fr;
    float rl;
    float rr;
};

inline QuadGains ComputeGains(SpatialPosition pos) {
    const SpatialPosition p = ClampField(pos);
    const float u = (p.x + 1.0f) * 0.5f;
    const float v = (p.y + 1.0f) * 0.5f;

    const float one_minus_u = 1.0f - u;
    const float one_minus_v = 1.0f - v;

    QuadGains g;
    g.fl = sqrtf(one_minus_u * one_minus_v);
    g.fr = sqrtf(u           * one_minus_v);
    g.rl = sqrtf(one_minus_u * v);
    g.rr = sqrtf(u           * v);
    return g;
}

}  // namespace qbm
