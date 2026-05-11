// =============================================================================
// SpatialPos.h
//
// In C++, code is split between HEADER files (.h) and SOURCE files (.cpp).
// A header declares things; a source file defines them. Other files "import"
// a header by writing `#include "SpatialPos.h"` — Python's `import` analogue.
//
// `#pragma once` is the modern way to say "include this file at most once per
// translation unit". Without it, the compiler would paste the file contents
// multiple times and complain about duplicate definitions. Python doesn't need
// this because `import` already de-dupes.
//
// This file is HEADER-ONLY: everything is `inline` so we don't need a matching
// .cpp file. That's fine for very small utilities.
// =============================================================================
#pragma once

// `namespace qbm { ... }` is like wrapping everything in a Python package.
// `qbm` = "quad boids mixer". Code outside the namespace must write
// `qbm::SpatialPosition` or do `using namespace qbm;` (like Python's
// `from qbm import *`) to use the names directly.
namespace qbm {

// `struct` is a class whose members are PUBLIC by default. In C++ the only
// difference between `struct` and `class` is the default visibility.
// Python equivalent: a tiny @dataclass with two float fields. No __init__ —
// you initialise with brace syntax: `SpatialPosition p = { 0.3f, -0.5f };`
struct SpatialPosition {
    float x;  // -1.0 = full left,  +1.0 = full right
    float y;  // -1.0 = front,      +1.0 = rear
};

// `inline` tells the compiler "it's OK to paste this function's body at the
// call site instead of doing a function call". For tiny helpers, this avoids
// the function-call overhead AND avoids "duplicate symbol" linker errors when
// the header is included in many .cpp files.
//
// The `float` before `Clamp` is the return type. The parameters all have
// their type spelled out — C++ has no Python-style duck typing. (See
// Smoother.h for `template` which is the closest C++ comes to duck typing.)
inline float Clamp(float v, float lo, float hi) {
    // C++ ternary: `cond ? a : b`. Same semantics as Python's `a if cond else b`.
    return v < lo ? lo : (v > hi ? hi : v);
}

// Pass-by-VALUE: `SpatialPosition p` copies the struct (16 bytes — cheap).
// We could pass by reference (`const SpatialPosition& p`) but for small structs
// copying is often faster than the indirection. Python passes everything by
// reference-to-object; C++ makes the choice explicit.
//
// The returned struct uses BRACE INITIALISATION: `{ ... }` constructs a value
// of the function's return type in-place. Equivalent to Python's
// `return SpatialPosition(x=..., y=...)` but no constructor is called.
inline SpatialPosition ClampField(SpatialPosition p) {
    return { Clamp(p.x, -1.0f, 1.0f), Clamp(p.y, -1.0f, 1.0f) };
}

}  // namespace qbm
// (Closing the namespace. The comment is a convention — long files would
// otherwise leave you guessing which `}` closes what.)
