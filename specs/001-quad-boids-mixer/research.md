# Research: Quadraphonic Boids Mixer

**Branch**: `001-quad-boids-mixer` | **Date**: 2026-05-10  
**Status**: Complete — all NEEDS CLARIFICATION resolved

---

## 1. Quadraphonic Pan Law

### Decision
**Bilinear equal-power decomposition** with square-root gains.

### Formula

Given a normalised 2D position **(x, y) ∈ [−1, 1]²**:

```
u = (1 + x) / 2          // 0 = full left, 1 = full right
v = (1 + y) / 2          // 0 = front, 1 = rear

G_FL = √( (1−u)(1−v) )   // front-left  corner: x=−1, y=−1
G_FR = √(  u   (1−v) )   // front-right corner: x=+1, y=−1
G_RL = √( (1−u)  v  )    // rear-left   corner: x=−1, y=+1
G_RR = √(  u     v  )    // rear-right  corner: x=+1, y=+1
```

**Proof of constant power**: G_FL² + G_FR² + G_RL² + G_RR² = (1−u)(1−v) + u(1−v) + (1−u)v + uv = 1 for all (x, y). No normalisation factor needed.

### Headroom

When multiple inputs converge on one corner all at full amplitude, outputs can sum to N× full scale. A **6 dB output headroom margin** (multiply all outputs by 0.5) is the minimum safe guard. 9–12 dB is more conservative for a four-input system.

### Implementation cost on Cortex-M7

6 `sqrtf` calls + 4 multiplies per position per audio sample — approximately 1% CPU per input channel. Negligible.

### Alternatives considered

| Approach | Why rejected |
|----------|-------------|
| VBAP (4-speaker) | Only two speakers active at a time — spatial jumps, conditional branching, less smooth |
| Tangent panning | Not truly equal-power; rarely used; no advantage |
| First-order Ambisonics decode | Reduces mathematically to the bilinear formula for a fixed quad layout — redundant overhead |

---

## 2. Boids Algorithm — Embedded ARM Implementation

### Decision
Two independent flocks, **N = 32 boids per flock**, updated at **100 Hz in the main loop** (not the audio callback). Flock centroids drive the spatial positions of Audio Inputs 3 and 4 respectively. Boundary handling: **soft attract-to-centre**.

### CPU budget analysis (STM32H750 @ 480 MHz)

| Component | Cycles @ 100 Hz | % of 4.8M cycle budget |
|-----------|----------------|------------------------|
| Flock A (N=32): O(N²) checks | ~75 × 32² ≈ 77k | 1.6% |
| Flock A: per-boid update | ~7,000 × 32 ≈ 224k | 4.7% |
| Flock B (N=32): same | ~301k | 6.3% |
| **Total boids** | **~602k cycles** | **~12.5%** |
| Audio callback (full load) | ~50k per block × 10k blocks/s ≈ 500M/s — but interrupt-driven, not main-loop | separate |

At N=32 per flock, boids consume roughly 12–15% of available main loop cycles. This leaves ample headroom for OLED rendering and UI. **Profiling on real hardware is mandatory** — these estimates carry ±30% variance from actual FPU/cache behaviour.

### Canonical rule weights

```
separation:  w = 2.0,  radius = 0.12 (normalised field units)
alignment:   w = 1.0,  radius = 0.35
cohesion:    w = 1.2,  radius = 0.35
max_velocity = 0.07 units/frame (at 100 Hz → 7 units/s, crosses the field in ~0.3 s)
max_force    = 0.03 units/frame
```

A small random wander force (0.01–0.03 units/frame) should be added to break long-term periodicity and satisfy SC-006 (non-repeating over a 5-minute window).

### Boundary handling

**Soft attract-to-centre** with a gentle restorative force:

```
centre_force = 0.02 × (Vec2(0, 0) − position)
```

This keeps all boids within the ±1.0 field without hard clamping and prevents position jumps that would create audible artefacts in the panning output.

### Flock-to-audio mapping

Each flock's **centroid** (mean position of all N boids) is used as the SpatialPosition for the assigned audio input.

- Flock A centroid → Audio Input 3 position
- Flock B centroid → Audio Input 4 position

Main loop writes centroids to a pair of `volatile float` position values; the audio callback reads them with one-pole IIR smoothing applied before use. Because positions are smoothed at ~10 Hz (the smoothing filter cutoff), any single-sample read tear is inaudible.

### Scalability path

If profiling reveals headroom problems: reduce N to 20 per flock (50 Hz minimum). If more than 64 boids are wanted later, implement a 4×4 spatial grid to reduce O(N²) to O(N).

### Alternatives considered

| Approach | Why rejected |
|----------|-------------|
| Single flock with sub-group metrics | Less independent movement for In3/In4; musical interest reduced |
| Wrapping boundaries (torus) | Position teleports create audible panning jumps |
| Elastic bounce boundaries | Can oscillate at boundaries; less smooth for audio |
| N > 48 at launch | Too close to profiling margin; start conservative |

---

## 3. libDaisy API — Daisy Patch Hardware Interface

### CV inputs

```cpp
// Call once per audio block in AudioCallback:
patch.ProcessAnalogControls();

// Read normalised value (0.0–1.0):
float cv1 = patch.GetKnobValue(DaisyPatch::CTRL_1);  // X for In1
float cv2 = patch.GetKnobValue(DaisyPatch::CTRL_2);  // Y for In1
float cv3 = patch.GetKnobValue(DaisyPatch::CTRL_3);  // X for In2
float cv4 = patch.GetKnobValue(DaisyPatch::CTRL_4);  // Y for In2

// Map 0..1 → −1..+1:
float x = cv1 * 2.0f - 1.0f;
```

### Encoder — ghosting avoidance (critical)

`Increment()` returns the same ±1 for the entire 1 ms debounce tick. Reading it more than once per tick in the main loop produces erratic behaviour. **Required pattern**: accumulate in the audio callback, read once in main loop.

```cpp
// Globals (audio and main loop share these):
volatile int32_t g_enc_delta  = 0;
volatile bool    g_enc_pressed = false;

// In AudioCallback (after ProcessDigitalControls):
g_enc_delta  += patch.encoder.Increment();
if (patch.encoder.RisingEdge()) g_enc_pressed = true;

// In main loop UI handler:
int32_t enc = g_enc_delta;  g_enc_delta = 0;
if (g_enc_pressed) { g_enc_pressed = false; /* handle press */ }
```

### OLED display — coord clamping (critical)

The OLED is 128×64 pixels, (0,0) at top-left, x right, y down. `DrawLine` uses unsigned arithmetic — passing a negative coordinate wraps to ~4 billion and freezes the Bresenham loop, causing a **hard firmware freeze**. Always clamp before drawing:

```cpp
auto clamp_x = [](int v){ return std::max(0, std::min(127, v)); };
auto clamp_y = [](int v){ return std::max(0, std::min(63,  v)); };

patch.display.DrawLine(clamp_x(x0), clamp_y(y0), clamp_x(x1), clamp_y(y1), true);
```

No `DrawPixel` exists — use `DrawLine(x, y, x, y, true)` for single pixels.

### Audio callback signature

```cpp
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
```

Buffers are **separate arrays per channel** (not interleaved):
- `in[0..3]`: float arrays of length `size` (4 inputs)
- `out[0..3]`: float arrays of length `size` (4 outputs)
- `size`: block size in samples — typically 48 at 48 kHz

### Recommended main loop structure

```cpp
int main() {
    patch.Init();
    patch.StartAdc();
    patch.StartAudio(AudioCallback);
    while (1) {
        // UI: read accumulated encoder, update path selection
        // Boids update (~100 Hz, throttled by elapsed time check)
        // Display update (~30 fps, throttled)
    }
}
```

### Key warnings

| Risk | Mitigation |
|------|-----------|
| DrawLine hard freeze on negative coords | Clamp all coordinates to [0,127] × [0,63] before every DrawLine call |
| Encoder ghosting in main loop | Accumulate Increment() inside AudioCallback only |
| Heap allocation in callback | Static/stack only; audited at code review |
| Display Update() blocking | Call only in main loop, never in AudioCallback |
