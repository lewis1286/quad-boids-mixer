// =============================================================================
// PathSelector.h — owns one of each path type, exposes the active one
//
// Each automated audio input (In3, In4) has its own PathSelector. The user
// cycles through path types with the encoder; we never create or destroy
// path objects, just switch which one we consider "active".
//
// All three paths exist simultaneously and all could conceivably be Update'd
// — but we only call Update on the active one each tick. The inactive ones
// just sit in RAM. (~1 KB wasted per inactive boids flock — trivial.)
// =============================================================================
#pragma once

#include "BoidsPath.h"
#include "CircularPath.h"
#include "Figure8Path.h"
#include "PathBase.h"
#include "StaticPath.h"

namespace qbm {

class PathSelector {
public:
    // Default-construct: start at CIRCULAR.
    PathSelector() : active_(PathType::CIRCULAR) {}

    // Advance to the next path type. Switch statement is C++'s `match`-style
    // construct: `case PathType::CIRCULAR:` compares `active_` to that value
    // and runs the body if it matches.
    //
    // No `break` needed because each case ends with `break` (and would
    // otherwise fall through to the next case — a famous C++ footgun!).
    void Cycle() {
        switch (active_) {
            case PathType::CIRCULAR: active_ = PathType::FIGURE8;  break;
            case PathType::FIGURE8:  active_ = PathType::BOIDS;    break;
            case PathType::BOIDS:    active_ = PathType::STATIC;   break;
            case PathType::STATIC:   active_ = PathType::CIRCULAR; break;
        }
    }

    PathType Type() const { return active_; }

    // Returns a POINTER to the active path. `PathBase*` is "pointer to
    // PathBase". The caller uses arrow syntax: `selector.Active()->Update(dt)`.
    //
    // Why pointer and not reference? Because we have multiple objects of
    // different concrete types and need to choose at runtime. A reference
    // must be bound at creation; a pointer can be re-pointed.
    //
    // Two versions of Active() — one non-const, one const. C++ requires this
    // overload pair when you want a mutable accessor for non-const callers
    // and a read-only accessor for const callers.
    PathBase* Active() {
        switch (active_) {
            case PathType::CIRCULAR: return &circular_;
            case PathType::FIGURE8:  return &figure8_;
            case PathType::BOIDS:    return &boids_;
            case PathType::STATIC:   return &static_;
        }
        return &circular_;
    }

    const PathBase* Active() const {
        switch (active_) {
            case PathType::CIRCULAR: return &circular_;
            case PathType::FIGURE8:  return &figure8_;
            case PathType::BOIDS:    return &boids_;
            case PathType::STATIC:   return &static_;
        }
        return &circular_;
    }

    // Direct accessor needed by main.cpp to call SetSeed during init.
    BoidsPath& Boids() { return boids_; }

private:
    // COMPOSITION: PathSelector OWNS one of each concrete path type.
    // All three are statically allocated, in-place, no heap.
    // Python equivalent: `self.circular = CircularPath()` etc.
    CircularPath circular_;
    Figure8Path  figure8_;
    BoidsPath    boids_;
    StaticPath   static_;
    PathType     active_;
};

}  // namespace qbm
