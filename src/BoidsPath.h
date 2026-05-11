#pragma once

#include <math.h>
#include <stdint.h>

#include "PathBase.h"

namespace qbm {

constexpr int N_BOIDS = 32;

struct Boid {
    float pos_x;
    float pos_y;
    float vel_x;
    float vel_y;
};

struct BoidsParams {
    float w_separation = 2.0f;
    float w_alignment  = 1.0f;
    float w_cohesion   = 1.2f;
    float r_separation = 0.12f;
    float r_alignment  = 0.35f;
    float r_cohesion   = 0.35f;
    float max_velocity = 0.07f;
    float max_force    = 0.03f;
    float centre_force = 0.02f;
    float wander_force = 0.02f;
};

class BoidsPath : public PathBase {
public:
    BoidsPath() : rate_(1.0f), rng_state_(0xC0FFEEu) {
        // Scatter boids in [-0.5, +0.5]^2 with small random velocity.
        for (int i = 0; i < N_BOIDS; ++i) {
            boids_[i].pos_x = RandRange(-0.5f, 0.5f);
            boids_[i].pos_y = RandRange(-0.5f, 0.5f);
            boids_[i].vel_x = RandRange(-0.02f, 0.02f);
            boids_[i].vel_y = RandRange(-0.02f, 0.02f);
        }
    }

    // Seed each instance differently so the two flocks don't move in lock-step.
    void SetSeed(uint32_t seed) {
        rng_state_ = seed ? seed : 0xC0FFEEu;
        for (int i = 0; i < N_BOIDS; ++i) {
            boids_[i].pos_x = RandRange(-0.5f, 0.5f);
            boids_[i].pos_y = RandRange(-0.5f, 0.5f);
            boids_[i].vel_x = RandRange(-0.02f, 0.02f);
            boids_[i].vel_y = RandRange(-0.02f, 0.02f);
        }
    }

    void Update(float dt) override {
        const BoidsParams& p = params_;

        const float r_sep_sq   = p.r_separation * p.r_separation;
        const float r_align_sq = p.r_alignment  * p.r_alignment;
        const float r_coh_sq   = p.r_cohesion   * p.r_cohesion;

        // Computed steering for each boid this tick, applied after the loop
        // so all reads see the previous-tick state.
        float steer_x[N_BOIDS];
        float steer_y[N_BOIDS];

        for (int i = 0; i < N_BOIDS; ++i) {
            float sep_x = 0.0f, sep_y = 0.0f;
            float align_x = 0.0f, align_y = 0.0f;
            int   align_count = 0;
            float coh_x = 0.0f, coh_y = 0.0f;
            int   coh_count = 0;

            const float pix = boids_[i].pos_x;
            const float piy = boids_[i].pos_y;

            for (int j = 0; j < N_BOIDS; ++j) {
                if (j == i) continue;
                const float dx  = boids_[j].pos_x - pix;
                const float dy  = boids_[j].pos_y - piy;
                const float dsq = dx * dx + dy * dy;
                if (dsq <= 0.0f) continue;

                if (dsq < r_sep_sq) {
                    const float inv = 1.0f / dsq;  // weight by 1/distance²
                    sep_x -= dx * inv;
                    sep_y -= dy * inv;
                }
                if (dsq < r_align_sq) {
                    align_x += boids_[j].vel_x;
                    align_y += boids_[j].vel_y;
                    ++align_count;
                }
                if (dsq < r_coh_sq) {
                    coh_x += boids_[j].pos_x;
                    coh_y += boids_[j].pos_y;
                    ++coh_count;
                }
            }

            float ax = p.w_separation * sep_x;
            float ay = p.w_separation * sep_y;

            if (align_count > 0) {
                ax += p.w_alignment * (align_x / static_cast<float>(align_count) - boids_[i].vel_x);
                ay += p.w_alignment * (align_y / static_cast<float>(align_count) - boids_[i].vel_y);
            }
            if (coh_count > 0) {
                const float target_x = coh_x / static_cast<float>(coh_count);
                const float target_y = coh_y / static_cast<float>(coh_count);
                ax += p.w_cohesion * (target_x - pix);
                ay += p.w_cohesion * (target_y - piy);
            }

            // Soft centre attraction — keeps flock inside the field without hard clamping.
            ax += p.centre_force * (0.0f - pix);
            ay += p.centre_force * (0.0f - piy);

            // Wander — small random perturbation breaks long-term periodicity.
            ax += p.wander_force * RandRange(-1.0f, 1.0f);
            ay += p.wander_force * RandRange(-1.0f, 1.0f);

            // Clamp steering force magnitude.
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

        // Integrate.
        for (int i = 0; i < N_BOIDS; ++i) {
            boids_[i].vel_x += steer_x[i];
            boids_[i].vel_y += steer_y[i];

            // Clamp speed.
            const float v_sq    = boids_[i].vel_x * boids_[i].vel_x
                                + boids_[i].vel_y * boids_[i].vel_y;
            const float vmax_sq = p.max_velocity * p.max_velocity;
            if (v_sq > vmax_sq) {
                const float s = p.max_velocity / sqrtf(v_sq);
                boids_[i].vel_x *= s;
                boids_[i].vel_y *= s;
            }

            boids_[i].pos_x += boids_[i].vel_x * dt * rate_ * 100.0f;
            boids_[i].pos_y += boids_[i].vel_y * dt * rate_ * 100.0f;

            // Boundary softening — reflect velocity component if outside the
            // 0.95 ring. Prevents accumulated drift past the field edge.
            if (boids_[i].pos_x >  0.95f && boids_[i].vel_x > 0.0f) boids_[i].vel_x = -boids_[i].vel_x;
            if (boids_[i].pos_x < -0.95f && boids_[i].vel_x < 0.0f) boids_[i].vel_x = -boids_[i].vel_x;
            if (boids_[i].pos_y >  0.95f && boids_[i].vel_y > 0.0f) boids_[i].vel_y = -boids_[i].vel_y;
            if (boids_[i].pos_y < -0.95f && boids_[i].vel_y < 0.0f) boids_[i].vel_y = -boids_[i].vel_y;
        }
    }

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
    // Tiny LCG — heap-free, deterministic, fast on Cortex-M7.
    float RandRange(float lo, float hi) {
        rng_state_ = rng_state_ * 1664525u + 1013904223u;
        const float u = static_cast<float>(rng_state_ & 0x00FFFFFFu)
                      / static_cast<float>(0x01000000u);
        return lo + (hi - lo) * u;
    }

    Boid        boids_[N_BOIDS];
    BoidsParams params_;
    float       rate_;
    uint32_t    rng_state_;
};

}  // namespace qbm
