#pragma once

#include <math.h>

#include "PathBase.h"

namespace qbm {

class CircularPath : public PathBase {
public:
    CircularPath() : phase_(0.0f), rate_(1.0f) {}

    void Update(float dt) override {
        constexpr float kTwoPi    = 6.28318530717958647692f;
        constexpr float kBaseFreq = 0.25f;  // Hz at rate=1.0 → 4 s per revolution
        phase_ += rate_ * kBaseFreq * dt * kTwoPi;
        while (phase_ > kTwoPi) phase_ -= kTwoPi;
        while (phase_ < 0.0f)   phase_ += kTwoPi;
    }

    SpatialPosition GetPosition() const override {
        return { cosf(phase_), sinf(phase_) };
    }

    void SetRate(float rate) override {
        rate_ = Clamp(rate, 0.1f, 4.0f);
    }

    float GetRate() const override { return rate_; }

private:
    float phase_;
    float rate_;
};

}  // namespace qbm
