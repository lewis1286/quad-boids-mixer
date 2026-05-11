#pragma once

#include "SpatialPos.h"

namespace qbm {

enum class PathType : unsigned int {
    CIRCULAR = 0,
    FIGURE8  = 1,
    BOIDS    = 2,
};

class PathBase {
public:
    virtual ~PathBase() {}
    virtual void            Update(float dt)         = 0;
    virtual SpatialPosition GetPosition() const      = 0;
    virtual void            SetRate(float rate)      = 0;
    virtual float           GetRate() const          = 0;
};

}  // namespace qbm
