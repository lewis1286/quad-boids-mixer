# Hardware I/O Contract: Quadraphonic Boids Mixer

**Module**: Electrosmith Daisy Patch  
**Date**: 2026-05-10

This contract defines the mapping between physical hardware signals and module functions. It is the authoritative reference for the signal routing diagram and must be kept in sync with any hardware changes.

---

## Audio Inputs

| Jack | Label | Function |
|------|-------|----------|
| AUDIO IN 1 | In 1 | CV-panned audio input — position driven by CV1 (X) and CV2 (Y) |
| AUDIO IN 2 | In 2 | CV-panned audio input — position driven by CV3 (X) and CV4 (Y) |
| AUDIO IN 3 | In 3 | Path-automated audio input — position driven by selected MovementPath A |
| AUDIO IN 4 | In 4 | Path-automated audio input — position driven by selected MovementPath B |

---

## Audio Outputs

| Jack | Label | Position in Quadraphonic Field |
|------|-------|-------------------------------|
| AUDIO OUT 1 | FL | Front-Left — field position (x=−1, y=−1) |
| AUDIO OUT 2 | FR | Front-Right — field position (x=+1, y=−1) |
| AUDIO OUT 3 | RL | Rear-Left — field position (x=−1, y=+1) |
| AUDIO OUT 4 | RR | Rear-Right — field position (x=+1, y=+1) |

**Signal convention**: Audio outputs are the weighted sum of all four inputs at each corner, scaled via the equal-power bilinear pan law. Nominal output level: same as input level when one source occupies a corner at full amplitude. Output headroom margin: −6 dB (×0.5) applied globally to accommodate simultaneous full-amplitude inputs.

---

## CV Inputs

| Jack | Label | Function | Value Mapping |
|------|-------|----------|---------------|
| CV IN 1 | CV1 | X position for Audio In 1 | 0–3.3 V ADC → 0.0–1.0 normalised → −1.0 to +1.0 field |
| CV IN 2 | CV2 | Y position for Audio In 1 | 0–3.3 V ADC → 0.0–1.0 normalised → −1.0 to +1.0 field |
| CV IN 3 | CV3 | X position for Audio In 2 | 0–3.3 V ADC → 0.0–1.0 normalised → −1.0 to +1.0 field |
| CV IN 4 | CV4 | Y position for Audio In 2 | 0–3.3 V ADC → 0.0–1.0 normalised → −1.0 to +1.0 field |

**CV range note**: The Daisy Patch ADC accepts 0–3.3 V. The Eurorack CV standard allows signals up to ±5 V. Over-range inputs are clamped to the ADC rail by the hardware input protection circuit; the firmware maps the usable 0–3.3 V range to the full field extent.

**Unpatched CV**: Floating ADC inputs read near 0 V on the Daisy Patch (pulled low). In 1 and In 2 will default to the field position (−1.0, −1.0) (front-left corner) when CVs are unpatched. Patching a manual offset or a CV source is required to move the signal from the corner.

---

## Gate / Trigger Inputs

Not used in v1. Reserved for future use (e.g., path reset trigger, path select trigger).

---

## CV Outputs

Not used in v1. Reserved for future use (e.g., boids position outputs for external CV use).

---

## Encoder

| Interaction | Function |
|-------------|----------|
| Rotate (any direction) | Adjust path speed/rate for the currently selected automated input |
| Short press | Cycle through path types for the currently selected automated input (CIRCULAR → FIGURE8 → BOIDS → CIRCULAR) |
| Long press (> 500 ms) | Toggle which automated input is being configured (In3 ↔ In4) |

---

## OLED Display — Quadraphonic Field Diagram

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ RL ●                                                           ● RR           │
│                                                                               │
│                           (boids/path markers                                 │
│                            animate here)                                      │
│                                                                               │
│ FL ●                                                           ● FR           │
│                                                                               │
│ Path: CIRCULAR    Rate: 1.0    [In3 selected]                                 │
└──────────────────────────────────────────────────────────────────────────────┘
```

**Screen layout** (128×64 pixels):
- Rows 0–49: quadraphonic field area (50 px tall, 128 px wide)
  - Corner labels at the four corners of the field area
  - Four animated markers (•) for the four audio inputs
  - X axis: audio field x=−1 → screen x=4, x=+1 → screen x=123 (4px padding)
  - Y axis: audio field y=+1 (rear) → screen y=2, y=−1 (front) → screen y=47 (drawn top=rear, bottom=front)
- Row 50: horizontal divider line
- Rows 51–63: status bar showing active path name, rate value, selected input indicator

**Marker shapes** (all drawn as filled single-pixel dots or 3×3 squares):
- Audio In 1: `1`
- Audio In 2: `2`
- Audio In 3: `3`
- Audio In 4: `4`

Or, if font rendering at field positions is too slow, use distinct dots + a legend in the status bar.

---

## Signal Flow Overview

```
AUDIO IN 1 ──→ ┐
AUDIO IN 2 ──→ │
AUDIO IN 3 ──→ │  QuadMixer (pan law per input)
AUDIO IN 4 ──→ ┘
                      │
          CV1–CV4 ──→ PositionSource (In1, In2)
  CircularPath/        │
  Figure8Path/  ──→   PositionSource (In3, In4)
  BoidsPath             │
                      ▼
              ┌───────────────────┐
              │  FL  FR  RL  RR  │ ──→ AUDIO OUT 1–4
              └───────────────────┘
```
