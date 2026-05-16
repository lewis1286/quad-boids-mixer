// =============================================================================
// PathBase.h — abstract interface for "movement paths"
//
// A movement path produces a SpatialPosition as a function of elapsed time.
// We have three concrete kinds: circular, figure-8, boids. PathBase is the
// shared interface they all implement, so the rest of the code can treat them
// uniformly (PathSelector keeps one of each and switches between them).
// =============================================================================
#pragma once

#include "SpatialPos.h"

namespace qbm {

// `enum class` is a STRONGLY-TYPED enum, introduced in C++11. The values
// (CIRCULAR etc) are namespaced under PathType — you must write
// `PathType::CIRCULAR`, not just `CIRCULAR`. This avoids name collisions and
// prevents accidental int conversion. Closest Python analogue: `enum.IntEnum`.
//
// `: unsigned int` is the UNDERLYING TYPE — the enum stores its value in a
// uint. We could omit this (defaults to int); spelling it out documents the
// memory layout.
enum class PathType : unsigned int {
    CIRCULAR = 0,
    FIGURE8  = 1,
    BOIDS    = 2,
    STATIC   = 3,
};

// This is an ABSTRACT BASE CLASS (Python: an ABC with @abstractmethod).
// You cannot construct one directly; you can only construct subclasses that
// implement every pure-virtual method.
class PathBase {
public:
    // `virtual ... = 0;` declares a PURE VIRTUAL function. "Pure" = "has no
    // body in this class — subclasses MUST override it". Equivalent to:
    //
    //     class PathBase(ABC):
    //         @abstractmethod
    //         def update(self, dt): ...
    //
    // `virtual` (without `= 0`) would mean "you CAN override but don't have
    // to". `= 0` makes it mandatory.
    //
    // Without `virtual` C++ uses STATIC DISPATCH: calling `path.Update(dt)`
    // through a base pointer would call PathBase::Update, NOT the subclass
    // version. `virtual` enables DYNAMIC dispatch — what Python does by
    // default for every method. There is a small runtime cost (one extra
    // pointer indirection per call), which is why C++ makes it opt-in.
    //
    // `virtual ~PathBase() {}` is a VIRTUAL DESTRUCTOR. If you ever
    // `delete` an object through a base-class pointer, the destructor must
    // be virtual or you get undefined behaviour. We never `delete` paths in
    // this code (all are static), but it's a near-mandatory habit when
    // inheritance is involved.
    virtual ~PathBase() {}

    // The interface. Three pure-virtuals every concrete path must implement.
    virtual void            Update(float dt)         = 0;
    virtual SpatialPosition GetPosition() const      = 0;
    virtual void            SetRate(float rate)      = 0;
    virtual float           GetRate() const          = 0;
};

}  // namespace qbm
