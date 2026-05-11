#pragma once

#include "BoidsPath.h"
#include "CircularPath.h"
#include "Figure8Path.h"
#include "PathBase.h"

namespace qbm {

class PathSelector {
public:
    PathSelector() : active_(PathType::CIRCULAR) {}

    void Cycle() {
        switch (active_) {
            case PathType::CIRCULAR: active_ = PathType::FIGURE8;  break;
            case PathType::FIGURE8:  active_ = PathType::BOIDS;    break;
            case PathType::BOIDS:    active_ = PathType::CIRCULAR; break;
        }
    }

    PathType  Type() const { return active_; }

    PathBase* Active() {
        switch (active_) {
            case PathType::CIRCULAR: return &circular_;
            case PathType::FIGURE8:  return &figure8_;
            case PathType::BOIDS:    return &boids_;
        }
        return &circular_;
    }

    const PathBase* Active() const {
        switch (active_) {
            case PathType::CIRCULAR: return &circular_;
            case PathType::FIGURE8:  return &figure8_;
            case PathType::BOIDS:    return &boids_;
        }
        return &circular_;
    }

    BoidsPath& Boids() { return boids_; }

private:
    CircularPath circular_;
    Figure8Path  figure8_;
    BoidsPath    boids_;
    PathType     active_;
};

}  // namespace qbm
