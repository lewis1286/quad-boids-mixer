#pragma once

namespace qbm {

template <typename T>
class Smoother {
public:
    Smoother() : alpha_(0.001f), state_(T{}) {}
    explicit Smoother(T alpha) : alpha_(alpha), state_(T{}) {}

    void SetAlpha(T alpha) { alpha_ = alpha; }
    void Reset(T value)    { state_ = value; }

    T Process(T target) {
        state_ += alpha_ * (target - state_);
        return state_;
    }

    T Value() const { return state_; }

private:
    T alpha_;
    T state_;
};

}  // namespace qbm
