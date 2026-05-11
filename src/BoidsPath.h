// =============================================================================
// BoidsPath.h — Reynolds boids flocking simulation
//
// 32 virtual agents move around the field obeying three rules:
//   - separation: steer away from very close neighbours
//   - alignment:  steer toward the average velocity of nearby neighbours
//   - cohesion:   steer toward the average position of nearby neighbours
// Plus a small random wander, a soft pull toward the centre, and edge
// reflection. The MEAN POSITION of the flock is what we use as the spatial
// position for the audio source — so the audio moves like a flock instead of
// like a clockwork circle.
//
// Wikipedia "Boids" has a good intro to the canonical Reynolds rules.
// =============================================================================
#pragma once

#include <math.h>
#include <stdint.h>   // fixed-width integer types: uint32_t, int32_t, etc.

#include "PathBase.h"

namespace qbm {

// File-scope constant. `constexpr int` here is essentially `static const int`
// — known at compile time, suitable for fixed-size array sizes.
constexpr int N_BOIDS = 32;

// 16 bytes per boid (four 32-bit floats). With N=32 boids per flock and two
// flocks, that's 1 KB total — trivial on this MCU's 512 KB of fast RAM.
struct Boid {
    float pos_x;
    float pos_y;
    float vel_x;
    float vel_y;
};

// Tunable parameters as a struct. The `= 2.0f` etc are DEFAULT MEMBER
// INITIALISERS (C++11). They set the default value used when a BoidsParams
// is default-constructed. Python equivalent: `field(default=2.0)` on a dataclass.
struct BoidsParams {
    float w_separation = 2.0f;   // weights
    float w_alignment  = 1.0f;
    float w_cohesion   = 1.2f;
    float r_separation = 0.12f;  // neighbour radii
    float r_alignment  = 0.35f;
    float r_cohesion   = 0.35f;
    float max_velocity = 0.07f;  // speed clamp (units per simulation tick)
    float max_force    = 0.03f;  // steering force clamp
    float centre_force = 0.02f;  // soft pull toward field origin
    float wander_force = 0.02f;  // random perturbation magnitude
};

// Inherits PathBase like CircularPath/Figure8Path, but with substantially
// more state and computation.
class BoidsPath : public PathBase {
public:
    // Constructor: init rate and RNG seed, then scatter boids randomly.
    // We init rng_state_ first because RandRange() reads/writes it.
    //
    // CAUTION on member-init-list ORDER: members are initialised in
    // DECLARATION ORDER (the order they appear at the bottom of this class),
    // not in the order you list them here. The compiler will warn if they
    // disagree. We've written the list in declaration order to stay safe.
    BoidsPath() : rate_(1.0f), rng_state_(0xC0FFEEu) {
        // 0xC0FFEEu — hex literal with `u` suffix marking it `unsigned`.
        // `0x` prefix = base 16 (like Python's `0x` too).
        for (int i = 0; i < N_BOIDS; ++i) {
            boids_[i].pos_x = RandRange(-0.5f, 0.5f);
            boids_[i].pos_y = RandRange(-0.5f, 0.5f);
            boids_[i].vel_x = RandRange(-0.02f, 0.02f);
            boids_[i].vel_y = RandRange(-0.02f, 0.02f);
        }
    }

    // Lets main.cpp give each flock a distinct starting state so the two
    // flocks don't move in lock-step.
    void SetSeed(uint32_t seed) {
        rng_state_ = seed ? seed : 0xC0FFEEu;
        for (int i = 0; i < N_BOIDS; ++i) {
            boids_[i].pos_x = RandRange(-0.5f, 0.5f);
            boids_[i].pos_y = RandRange(-0.5f, 0.5f);
            boids_[i].vel_x = RandRange(-0.02f, 0.02f);
            boids_[i].vel_y = RandRange(-0.02f, 0.02f);
        }
    }

    // Main per-tick update: O(N^2) neighbour scan, then integrate.
    void Update(float dt) override {
        // `const BoidsParams& p = params_;` — `&` makes `p` a REFERENCE
        // (Python-like — `p` is an alias for the same object, no copy).
        // `const` means we won't mutate through it. Convenient shorthand
        // since we read `params_.X` many times below.
        const BoidsParams& p = params_;

        // Squared radii so we can compare distance-squared against them
        // (avoids 192 sqrtf calls per tick — sqrt is expensive).
        const float r_sep_sq   = p.r_separation * p.r_separation;
        const float r_align_sq = p.r_alignment  * p.r_alignment;
        const float r_coh_sq   = p.r_cohesion   * p.r_cohesion;

        // Stack-allocated scratch arrays — bytes come from the function's
        // stack frame, not the heap. With N=32 floats that's 128 bytes
        // per array; trivial on a 512 KB stack. Python has no equivalent;
        // every list is on the heap.
        //
        // We compute steering for EVERY boid based on the previous tick's
        // state before applying any of them, so order-of-iteration doesn't
        // bias the result.
        float steer_x[N_BOIDS];
        float steer_y[N_BOIDS];

        // Outer loop: each boid i computes its desired steering force.
        for (int i = 0; i < N_BOIDS; ++i) {
            // Per-boid accumulators. `int align_count = 0;` etc.
            float sep_x = 0.0f, sep_y = 0.0f;
            float align_x = 0.0f, align_y = 0.0f;
            int   align_count = 0;
            float coh_x = 0.0f, coh_y = 0.0f;
            int   coh_count = 0;

            // Pull boid i's position into a local once — micro-optimisation
            // for the inner loop. The compiler can probably do this itself,
            // but it's clearer too.
            const float pix = boids_[i].pos_x;
            const float piy = boids_[i].pos_y;

            // Inner loop: scan every OTHER boid j for this boid's neighbours.
            for (int j = 0; j < N_BOIDS; ++j) {
                if (j == i) continue;  // skip self
                const float dx  = boids_[j].pos_x - pix;
                const float dy  = boids_[j].pos_y - piy;
                const float dsq = dx * dx + dy * dy;  // distance SQUARED
                if (dsq <= 0.0f) continue;            // avoid divide-by-zero

                // Separation: push away, weighted by 1/distance^2 so very
                // close neighbours dominate. `inv = 1/dsq`; subtract dx*inv
                // because we want to move OPPOSITE to the neighbour direction.
                if (dsq < r_sep_sq) {
                    const float inv = 1.0f / dsq;
                    sep_x -= dx * inv;
                    sep_y -= dy * inv;
                }
                // Alignment: sum of neighbour VELOCITIES (we'll average later).
                if (dsq < r_align_sq) {
                    align_x += boids_[j].vel_x;
                    align_y += boids_[j].vel_y;
                    ++align_count;
                }
                // Cohesion: sum of neighbour POSITIONS (we'll average later).
                if (dsq < r_coh_sq) {
                    coh_x += boids_[j].pos_x;
                    coh_y += boids_[j].pos_y;
                    ++coh_count;
                }
            }

            // Combine the three rules into a single steering vector (ax, ay).
            float ax = p.w_separation * sep_x;
            float ay = p.w_separation * sep_y;

            // Alignment: steer toward neighbours' AVERAGE velocity minus our
            // own — the standard formulation.
            if (align_count > 0) {
                ax += p.w_alignment * (align_x / static_cast<float>(align_count) - boids_[i].vel_x);
                ay += p.w_alignment * (align_y / static_cast<float>(align_count) - boids_[i].vel_y);
            }
            // Cohesion: steer toward the centroid of nearby neighbours.
            // `static_cast<float>(int)` is C++'s explicit type cast — the
            // closest Python analogue is `float(x)`.
            if (coh_count > 0) {
                const float target_x = coh_x / static_cast<float>(coh_count);
                const float target_y = coh_y / static_cast<float>(coh_count);
                ax += p.w_cohesion * (target_x - pix);
                ay += p.w_cohesion * (target_y - piy);
            }

            // Soft attraction back toward field origin — prevents the flock
            // from drifting permanently into one corner.
            ax += p.centre_force * (0.0f - pix);
            ay += p.centre_force * (0.0f - piy);

            // Wander: a small random kick on every tick. This breaks any
            // long-term periodicity that the deterministic rules would
            // otherwise produce, satisfying spec criterion SC-006 (movement
            // must be non-repeating over a 5-minute window).
            ax += p.wander_force * RandRange(-1.0f, 1.0f);
            ay += p.wander_force * RandRange(-1.0f, 1.0f);

            // Cap the steering force magnitude at max_force. Without this,
            // any single tick could fling a boid across the field.
            const float mag_sq = ax * ax + ay * ay;
            const float maxf_sq = p.max_force * p.max_force;
            if (mag_sq > maxf_sq) {
                const float scale = p.max_force / sqrtf(mag_sq);
                ax *= scale;
                ay *= scale;
            }

            steer_x[i] = ax;
            steer_y[i] = ay;
        }

        // Second pass — apply steering, clamp speed, integrate position.
        // Reading and writing in two passes is critical for stability;
        // mixing them in the first loop would mean later boids see partially
        // updated earlier boids.
        for (int i = 0; i < N_BOIDS; ++i) {
            boids_[i].vel_x += steer_x[i];
            boids_[i].vel_y += steer_y[i];

            // Speed clamp via squared comparison.
            const float v_sq    = boids_[i].vel_x * boids_[i].vel_x
                                + boids_[i].vel_y * boids_[i].vel_y;
            const float vmax_sq = p.max_velocity * p.max_velocity;
            if (v_sq > vmax_sq) {
                const float s = p.max_velocity / sqrtf(v_sq);
                boids_[i].vel_x *= s;
                boids_[i].vel_y *= s;
            }

            // Integrate position. The `* 100.0f` is the unit-conversion
            // factor: our velocity values are "units per 10 ms tick", and
            // we receive `dt` in seconds, so we multiply by 100 to convert.
            boids_[i].pos_x += boids_[i].vel_x * dt * rate_ * 100.0f;
            boids_[i].pos_y += boids_[i].vel_y * dt * rate_ * 100.0f;

            // Boundary handling — if a boid passes the 0.95 ring moving
            // outward, reflect its velocity component. Soft (no hard
            // clamp), so position can briefly exceed 0.95 but it'll
            // return. Prevents accumulating drift past the field edge.
            if (boids_[i].pos_x >  0.95f && boids_[i].vel_x > 0.0f) boids_[i].vel_x = -boids_[i].vel_x;
            if (boids_[i].pos_x < -0.95f && boids_[i].vel_x < 0.0f) boids_[i].vel_x = -boids_[i].vel_x;
            if (boids_[i].pos_y >  0.95f && boids_[i].vel_y > 0.0f) boids_[i].vel_y = -boids_[i].vel_y;
            if (boids_[i].pos_y < -0.95f && boids_[i].vel_y < 0.0f) boids_[i].vel_y = -boids_[i].vel_y;
        }
    }

    // The position we expose to the rest of the system is the FLOCK MEAN —
    // i.e. the centroid. Move every boid in lock-step and the centroid moves
    // the same way; if they scatter, the centroid stays roughly central.
    SpatialPosition GetPosition() const override {
        float sx = 0.0f, sy = 0.0f;
        for (int i = 0; i < N_BOIDS; ++i) {
            sx += boids_[i].pos_x;
            sy += boids_[i].pos_y;
        }
        const float inv_n = 1.0f / static_cast<float>(N_BOIDS);
        return ClampField({ sx * inv_n, sy * inv_n });
    }

    void  SetRate(float rate) override { rate_ = Clamp(rate, 0.1f, 4.0f); }
    float GetRate() const     override { return rate_; }

private:
    // Tiny Linear Congruential Generator. Not crypto-grade — perfect for
    // wander noise. Multiply-and-add mixing constants are from Numerical
    // Recipes. Heap-free, deterministic, and ~5 cycles per call.
    float RandRange(float lo, float hi) {
        rng_state_ = rng_state_ * 1664525u + 1013904223u;
        // Take 24 bits and normalise to [0, 1). `0x00FFFFFFu` is a 24-bit mask.
        const float u = static_cast<float>(rng_state_ & 0x00FFFFFFu)
                      / static_cast<float>(0x01000000u);
        return lo + (hi - lo) * u;
    }

    // Members — declaration ORDER matters for the constructor init list above.
    Boid        boids_[N_BOIDS];   // C-style fixed-size array
    BoidsParams params_;           // default-constructed -> uses the
                                   // default member initialisers we wrote above
    float       rate_;
    uint32_t    rng_state_;
};

}  // namespace qbm
