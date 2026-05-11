# Data Model: Quadraphonic Boids Mixer

**Branch**: `001-quad-boids-mixer` | **Date**: 2026-05-10

---

## Entities

### SpatialPosition

A 2D coordinate in the normalised quadraphonic field.

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `x` | float | [−1.0, +1.0] | Horizontal axis: −1 = full left, +1 = full right |
| `y` | float | [−1.0, +1.0] | Depth axis: −1 = front, +1 = rear |

**Corner mapping**:

| Corner | x | y |
|--------|---|---|
| FL (front-left) | −1 | −1 |
| FR (front-right) | +1 | −1 |
| RL (rear-left) | −1 | +1 |
| RR (rear-right) | +1 | +1 |

---

### AudioInput

One of four mono audio sources entering the module. Each input has a current SpatialPosition that is updated every audio block.

| Field | Type | Description |
|-------|------|-------------|
| `index` | int | 1–4 identifies the input |
| `position` | SpatialPosition | Current position in the quadraphonic field |
| `smoothed_x` | float | One-pole IIR state for x (prevents zipper noise) |
| `smoothed_y` | float | One-pole IIR state for y |
| `source` | PositionSource | CV_CONTROLLED (In1, In2) or PATH_CONTROLLED (In3, In4) |

**State transitions**:

```
CV_CONTROLLED inputs: position updated every block from ADC values via IIR smoother
PATH_CONTROLLED inputs: position updated from MovementPath output via IIR smoother
```

---

### QuadGains

The four amplitude multipliers computed from a SpatialPosition via the pan law. Recomputed each audio block per input.

| Field | Type | Description |
|-------|------|-------------|
| `fl` | float | [0.0, 1.0] Front-left gain |
| `fr` | float | [0.0, 1.0] Front-right gain |
| `rl` | float | [0.0, 1.0] Rear-left gain |
| `rr` | float | [0.0, 1.0] Rear-right gain |

**Invariant**: `fl² + fr² + rl² + rr² = 1.0` (constant power, equal-power pan law).

**Pan law formula**:
```
u = (pos.x + 1) / 2
v = (pos.y + 1) / 2
fl = √((1−u)(1−v)),  fr = √(u(1−v))
rl = √((1−u)v),       rr = √(u·v)
```

---

### MovementPath (abstract)

A trajectory generator that produces a SpatialPosition as a function of elapsed time. Concrete subtypes implement the `Update(dt)` method.

| Field | Type | Description |
|-------|------|-------------|
| `type` | PathType | CIRCULAR, FIGURE8, or BOIDS |
| `rate` | float | Movement speed multiplier (default 1.0) |
| `position` | SpatialPosition | Most recently computed output position |

**Subtypes**:

#### CircularPath
Produces circular motion using a single phase accumulator.

| Field | Type | Description |
|-------|------|-------------|
| `phase` | float | Current angle in radians, [0, 2π) |
| `phase_inc` | float | Radians per update tick (= 2π × freq × dt) |

Output: `x = cos(phase)`, `y = sin(phase)`

#### Figure8Path
Produces a Lissajous figure-8 using a 1:2 frequency ratio.

| Field | Type | Description |
|-------|------|-------------|
| `phase` | float | Current angle in radians |
| `phase_inc` | float | Radians per update tick |

Output: `x = sin(phase)`, `y = sin(2 × phase)`

#### BoidsPath
Runs a flock of N boids and exposes the flock centroid as its output position.

| Field | Type | Description |
|-------|------|-------------|
| `boids` | Boid[N_BOIDS] | Static array of boid agents (N_BOIDS = 32) |
| `centroid` | SpatialPosition | Running mean position of all boids |
| `params` | BoidsParams | Weights and radii (see below) |

---

### Boid

A single autonomous agent in the boids flocking simulation.

| Field | Type | Description |
|-------|------|-------------|
| `pos_x` | float | X position in field [−1.0, +1.0] |
| `pos_y` | float | Y position in field [−1.0, +1.0] |
| `vel_x` | float | X velocity (units/tick) |
| `vel_y` | float | Y velocity (units/tick) |

**Memory**: 4 floats × 32 boids × 2 flocks = 256 floats = 1,024 bytes (trivial)

---

### BoidsParams

Tuning parameters for the boids simulation. All are fixed at compile time for v1.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `w_separation` | float | 2.0 | Separation rule weight |
| `w_alignment` | float | 1.0 | Alignment rule weight |
| `w_cohesion` | float | 1.2 | Cohesion rule weight |
| `r_separation` | float | 0.12 | Separation neighbourhood radius |
| `r_alignment` | float | 0.35 | Alignment neighbourhood radius |
| `r_cohesion` | float | 0.35 | Cohesion neighbourhood radius |
| `max_velocity` | float | 0.07 | Maximum speed (units/tick) |
| `max_force` | float | 0.03 | Maximum steering force (units/tick²) |
| `centre_force` | float | 0.02 | Soft attraction toward field centre |
| `wander_force` | float | 0.02 | Small random perturbation magnitude |

---

### PathSelector

Manages the currently selected path for one audio input (In3 or In4).

| Field | Type | Description |
|-------|------|-------------|
| `active_type` | PathType | Currently selected path (CIRCULAR, FIGURE8, BOIDS) |
| `circular` | CircularPath | Instance (always allocated, statically) |
| `figure8` | Figure8Path | Instance (always allocated, statically) |
| `boids` | BoidsPath | Instance (always allocated, statically) |

**State transition** (encoder press cycles through: CIRCULAR → FIGURE8 → BOIDS → CIRCULAR):

```
Encoder press on In3 selector → active_type = (active_type + 1) % 3
Encoder press on In4 selector → active_type = (active_type + 1) % 3
```

---

### UIState

Module-level interaction state, managed in the main loop.

| Field | Type | Description |
|-------|------|-------------|
| `selected_input` | int | Which automated input is being configured (3 or 4) |
| `path3` | PathSelector | Path selector for Audio In3 |
| `path4` | PathSelector | Path selector for Audio In4 |
| `enc_delta` | volatile int32 | Accumulated encoder rotation (updated in callback) |
| `enc_pressed` | volatile bool | Encoder press flag (updated in callback) |

---

### SharedPositions

Double-buffer (or volatile float) bridge between the main loop (path/boids update) and the audio callback (pan law application).

| Field | Type | Description |
|-------|------|-------------|
| `pos_in3_x` | volatile float | Most recent X position for In3, written by main loop |
| `pos_in3_y` | volatile float | Most recent Y position for In3, written by main loop |
| `pos_in4_x` | volatile float | Most recent X position for In4, written by main loop |
| `pos_in4_y` | volatile float | Most recent Y position for In4, written by main loop |

One-pole IIR smoothing in the audio callback makes any single-sample read tear inaudible.

---

## Relationships

```
UIState
  ├─ PathSelector (In3)
  │    ├─ CircularPath
  │    ├─ Figure8Path
  │    └─ BoidsPath ──── Boid[32]
  └─ PathSelector (In4)
       ├─ CircularPath
       ├─ Figure8Path
       └─ BoidsPath ──── Boid[32]

SharedPositions  ←── written by active MovementPath (main loop)
                 ──→ read by AudioCallback → QuadGains → output[0..3]

AudioInput[1..4]
  ├─ [1,2]: SpatialPosition ← CV1–CV4 (via IIR smoother in callback)
  └─ [3,4]: SpatialPosition ← SharedPositions (via IIR smoother in callback)
```

---

## Memory Estimate

| Entity | Size | Count | Total |
|--------|------|-------|-------|
| Boid (4 floats) | 16 B | 64 (2 flocks) | 1,024 B |
| Path states | ~32 B | 6 paths (3 per input) | ~192 B |
| UIState + PathSelector | ~64 B | 1 | 64 B |
| SharedPositions | 16 B | 1 | 16 B |
| Smoothing state (8 floats) | 32 B | 1 | 32 B |
| **Total data model** | | | **~1.3 KB** |

Well within DTCM budget (512 KB, 80% limit = 410 KB).
