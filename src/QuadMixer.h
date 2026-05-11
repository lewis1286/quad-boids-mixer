// =============================================================================
// QuadMixer.h — the four-speaker pan law
//
// Given a position (x, y) in the field, this computes the four output gains
// that, when applied to a mono source, place that source spatially in a
// quad speaker array.
//
// The math is "bilinear equal-power": all four squared gains always sum to 1,
// which means perceived loudness stays constant as the source moves. See
// research.md §1 for the derivation.
// =============================================================================
#pragma once

#include <math.h>      // sqrtf — angle-bracket include = standard / system header
                       // (#include "x.h" = local header, #include <x.h> = system)
#include "SpatialPos.h"

namespace qbm {

// Plain data struct — same idea as SpatialPosition. Four output amplitudes.
// Python equivalent: `@dataclass class QuadGains: fl, fr, rl, rr: float`.
struct QuadGains {
    float fl;  // front-left
    float fr;  // front-right
    float rl;  // rear-left
    float rr;  // rear-right
};

// FREE FUNCTION (not a method on a class). C++ is happy with free functions;
// you do not need to wrap everything in a class like Java or strict OO Python.
//
// `const SpatialPosition p` would also work — `const` on a value parameter
// just means "I won't modify my local copy", and is mostly stylistic. We
// could also write `const SpatialPosition& pos` (a const reference) to avoid
// the copy, but at 8 bytes it's not worth the indirection.
inline QuadGains ComputeGains(SpatialPosition pos) {
    // Clamp first so we never sqrtf a negative number.
    const SpatialPosition p = ClampField(pos);

    // Remap [-1, +1] -> [0, 1]. `u` and `v` are the standard names for
    // normalised 2D coordinates in graphics. `0.5f` (not `0.5`) is a float
    // literal — without the `f` suffix it would be a double and the multiply
    // would promote everything to double, which is much slower on the
    // STM32H750 (single-precision FPU is hardware, double is software-emulated).
    const float u = (p.x + 1.0f) * 0.5f;
    const float v = (p.y + 1.0f) * 0.5f;

    // We need (1-u), (1-v) twice each — compute once and reuse. The compiler
    // would probably do this anyway, but it costs nothing to be explicit and
    // it makes the symmetry of the math visible.
    const float one_minus_u = 1.0f - u;
    const float one_minus_v = 1.0f - v;

    // Build the return value. `QuadGains g;` default-constructs (all fields
    // are uninitialised garbage at this point — C++ doesn't zero memory like
    // Python does). We then assign each field, then return by value.
    QuadGains g;
    g.fl = sqrtf(one_minus_u * one_minus_v);  // corner: x=-1, y=-1
    g.fr = sqrtf(u           * one_minus_v);  // corner: x=+1, y=-1
    g.rl = sqrtf(one_minus_u * v);            // corner: x=-1, y=+1
    g.rr = sqrtf(u           * v);            // corner: x=+1, y=+1
    return g;
}

}  // namespace qbm
