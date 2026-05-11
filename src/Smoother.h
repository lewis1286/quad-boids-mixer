// =============================================================================
// Smoother.h — one-pole IIR (infinite impulse response) parameter smoother
//
// A "smoother" is the classic fix for "zipper noise" in audio: if you stream
// a raw control value (e.g. a knob) directly into a gain, every tiny jitter
// produces an audible step. Running it through an IIR low-pass smears those
// steps over ~100 ms so they become inaudible.
//
// The math is one line:    state = state + alpha * (target - state)
// alpha=1 means "follow instantly"; alpha=0 means "never move".
// =============================================================================
#pragma once

namespace qbm {

// `template <typename T>` is C++'s generic-types feature. It is loosely like
// Python's duck typing, but resolved at COMPILE TIME: every concrete `T` you
// use produces a separate, fully type-checked copy of the class. So
// `Smoother<float>` and `Smoother<double>` are two distinct types.
//
// In Python you'd just write a class that works on anything supporting `+ *`
// and trust the runtime to fail if you pass something weird. In C++ the
// compiler checks each instantiation and refuses to build if `T` doesn't
// support the operations used inside.
template <typename T>
class Smoother {
public:
    // `Smoother()` is the DEFAULT CONSTRUCTOR — called when you write
    // `Smoother<float> s;` with no arguments. Python equivalent: `__init__`.
    //
    // The `: alpha_(0.001f), state_(T{})` is the MEMBER INITIALISER LIST. It
    // initialises member variables BEFORE the function body runs. `T{}` means
    // "default-construct a T" (for float, that's 0.0). This is the idiomatic
    // way to initialise members; assigning inside the body is slower for
    // non-trivial types.
    Smoother() : alpha_(0.001f), state_(T{}) {}

    // `explicit` prevents implicit conversion. Without it, `Smoother<float> s
    // = 0.5f;` would silently call this constructor. `explicit` means "you
    // must spell it out": `Smoother<float> s(0.5f);` or `Smoother<float> s{0.5f};`.
    explicit Smoother(T alpha) : alpha_(alpha), state_(T{}) {}

    // Single-line method bodies are conventionally inlined directly in the
    // class definition. Equivalent to Python's `def set_alpha(self, a): self.alpha_ = a`.
    void SetAlpha(T alpha) { alpha_ = alpha; }
    void Reset(T value)    { state_ = value; }

    // `T Process(T target)` — takes a target, returns the new smoothed value.
    // No `self` parameter: in C++, methods implicitly have access to the
    // instance via `this` (a pointer; analogous to Python's `self`).
    T Process(T target) {
        state_ += alpha_ * (target - state_);
        return state_;
    }

    // `T Value() const` — the `const` AFTER the parameter list means
    // "this method does not modify the object". You can call const methods on
    // const objects; you cannot call non-const methods on const objects.
    // Python has no equivalent — everything is mutable.
    T Value() const { return state_; }

// `private:` — members below this line are inaccessible from outside the class.
// C++ enforces this at compile time, unlike Python's "_underscore" convention
// which is just a hint.
private:
    T alpha_;  // The trailing underscore is a common naming convention for
    T state_;  // private members. C++ has no language-level convention.
};

}  // namespace qbm
