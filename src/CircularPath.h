// =============================================================================
// CircularPath.h — phase-accumulator circle generator
//
// Position is just (cos(phase), sin(phase)) — moves the source around the
// unit circle as phase increases. Phase wraps every 2*pi radians.
// =============================================================================
#pragma once

#include <math.h>
#include "PathBase.h"

namespace qbm {

// `class CircularPath : public PathBase` — INHERITANCE. CircularPath is a
// PathBase. The `public` controls how the inheritance is exposed; almost
// always `public` for "is-a" relationships. Python equivalent:
//
//     class CircularPath(PathBase):
//         ...
class CircularPath : public PathBase {
public:
    // Member init list: both members start at 0.0 / 1.0.
    CircularPath() : phase_(0.0f), rate_(1.0f) {}

    // `override` (a C++11 keyword) tells the compiler "I am intentionally
    // overriding a virtual from a base class". If the signature doesn't
    // match a base virtual, the compiler errors out. Very useful — without
    // `override`, a typo (`Updat` instead of `Update`) silently creates a
    // new method that never gets called. Python has no equivalent guard.
    void Update(float dt) override {
        // `constexpr` = "compile-time constant". Strictly stronger than
        // `const`: the value must be known at compile time. Python's
        // closest analogue is a module-level CONSTANT, but Python has no
        // way to enforce compile-time evaluation.
        constexpr float kTwoPi    = 6.28318530717958647692f;
        constexpr float kBaseFreq = 0.25f;  // 0.25 Hz -> 4 s per revolution
        phase_ += rate_ * kBaseFreq * dt * kTwoPi;

        // Wrap phase back into [0, 2*pi). `while` instead of `fmodf` because
        // it's typically faster when the values are close to the range.
        while (phase_ > kTwoPi) phase_ -= kTwoPi;
        while (phase_ < 0.0f)   phase_ += kTwoPi;
    }

    // `... const override` — both qualifiers. `const` = "doesn't mutate the
    // object"; `override` = "implements a base-class virtual".
    SpatialPosition GetPosition() const override {
        return { cosf(phase_), sinf(phase_) };
    }

    void SetRate(float rate) override {
        rate_ = Clamp(rate, 0.1f, 4.0f);
    }

    float GetRate() const override { return rate_; }

private:
    float phase_;  // radians
    float rate_;   // speed multiplier; 1.0 = base, 0.1..4.0 valid
};

}  // namespace qbm
