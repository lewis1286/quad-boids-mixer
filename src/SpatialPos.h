#pragma once

namespace qbm {

struct SpatialPosition {
    float x;
    float y;
};

inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline SpatialPosition ClampField(SpatialPosition p) {
    return { Clamp(p.x, -1.0f, 1.0f), Clamp(p.y, -1.0f, 1.0f) };
}

}  // namespace qbm
