#pragma once

#include "PathBase.h"

namespace qbm {

// Movement disabled — holds the center of the field (0, 0). Update() is a
// no-op; SetRate/GetRate are stubs so the interface is satisfied.
class StaticPath : public PathBase {
public:
    void            Update(float /*dt*/)    override {}
    SpatialPosition GetPosition()  const    override { return { 0.0f, 0.0f }; }
    void            SetRate(float /*rate*/) override {}
    float           GetRate()      const    override { return 0.0f; }
};

}  // namespace qbm
