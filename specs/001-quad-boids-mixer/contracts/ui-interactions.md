# UI Interaction Contract: Quadraphonic Boids Mixer

**Module**: Electrosmith Daisy Patch  
**Date**: 2026-05-10

This contract defines all user interactions with the module's encoder and OLED display. It is the authoritative reference for UI behaviour and must be kept in sync with the firmware implementation.

---

## Encoder Interactions

The Daisy Patch has one encoder with rotation (CW/CCW) and a push-button.

### Mode overview

The encoder operates in one mode with two sub-contexts: **In3 context** and **In4 context**. The current context determines which automated input's path and speed are being adjusted.

---

### Rotation (CW / CCW)

**In3 context**: Adjusts the path movement rate for Audio Input 3.  
**In4 context**: Adjusts the path movement rate for Audio Input 4.

| Direction | Effect |
|-----------|--------|
| CW (+1 increment) | Increase rate by one step (minimum step: 0.05×, range 0.1× – 4.0×) |
| CCW (−1 increment) | Decrease rate by one step |

Rate adjustments are bounded: turning below 0.1× holds at 0.1×; turning above 4.0× holds at 4.0×. Rate changes take effect immediately on the next path update tick.

---

### Short Press (< 500 ms)

Cycles the active path type for the currently selected input.

```
CIRCULAR → FIGURE8 → BOIDS → CIRCULAR → ...
```

Path switch happens at the end of the press (rising edge). The new path begins from its last-known phase/state immediately (no reset to start position), to minimise position jumps.

---

### Long Press (≥ 500 ms)

Toggles the selected context between In3 and In4.

```
[In3 selected] ──(long press)──→ [In4 selected]
[In4 selected] ──(long press)──→ [In3 selected]
```

The selected context is shown in the status bar on the OLED.

---

## Encoder State Diagram

```
Power-on
    │
    ▼
[In3 context, CIRCULAR, rate=1.0]
    │
    ├── CW rotation    ──→ rate++ (In3)
    ├── CCW rotation   ──→ rate-- (In3)
    ├── Short press    ──→ cycle path (In3): CIRCULAR → FIGURE8 → BOIDS → CIRCULAR
    └── Long press     ──→ [In4 context, CIRCULAR, rate=1.0]
                              │
                              ├── CW rotation    ──→ rate++ (In4)
                              ├── CCW rotation   ──→ rate-- (In4)
                              ├── Short press    ──→ cycle path (In4)
                              └── Long press     ──→ [In3 context]
```

---

## OLED Display Contract

### Display update rate

The display is refreshed in the main loop at a target of **20 fps** (every 50 ms). The OLED `Update()` call blocks while the SPI/I2C transfer completes; this is acceptable in the main loop but must never be called from the audio callback.

### Screen regions

```
y=0  ┌────────────────────────────────────────────────────────────┐
     │                                                            │  ← Field area
     │   RL                                      RR               │    rows 0–47
     │    •                                       •               │
     │                                                            │
     │        [animated markers for In1, In2, In3, In4]          │
     │    •                                       •               │
     │   FL                                      FR               │
y=47 │                                                            │
y=48 ├────────────────────────────────────────────────────────────┤  ← Divider
y=49 │ [In3] CIRCULAR  ×1.0  |  [In4] BOIDS  ×0.5               │  ← Status bar
     │                       ↑ selected input highlighted          │    rows 49–63
y=63 └────────────────────────────────────────────────────────────┘
```

### Field area (rows 0–47)

Coordinate mapping:
- Audio x=−1.0 → screen x=4; audio x=+1.0 → screen x=123 (4 px padding each side)
- Audio y=+1.0 (rear) → screen y=2; audio y=−1.0 (front) → screen y=45 (top=rear, bottom=front)

Formula:
```
screen_x = (int)((audio_x + 1.0f) * 0.5f * 119.0f) + 4
screen_y = (int)((1.0f - (audio_y + 1.0f) * 0.5f) * 43.0f) + 2
```

Corner labels drawn once at frame start:
- FL: text at screen (2, 38)
- FR: text at screen (110, 38)
- RL: text at screen (2, 4)
- RR: text at screen (110, 4)

Animated markers (one per input):
- Drawn as a 3×3 filled square centred on the screen position
- In1: `1`, In2: `2`, In3: `3`, In4: `4` (or simple filled squares if text is too slow)

### Status bar (rows 49–63)

Displays path name and rate for both In3 and In4. The currently selected input's entry is drawn in inverse video (white background, black text) or marked with `>`. Example:

```
>[In3]CIRC×1.0  [In4]BOID×0.5
```

Path name abbreviations: `CIRC`, `FIG8`, `BOID`.

Rate displayed to one decimal place (e.g., `×1.0`, `×0.5`, `×4.0`).

### Coordinate safety rule

All screen coordinates MUST be clamped to [0, 127] × [0, 63] before any `DrawLine` or text call. Failure to clamp will cause a firmware hard-freeze on the Daisy Patch (unsigned arithmetic wraps → Bresenham infinite loop). See research.md §3 for details.

---

## Power-on Default State

| Parameter | Default |
|-----------|---------|
| Selected context | In3 |
| In3 path | CIRCULAR |
| In3 rate | 1.0× |
| In4 path | CIRCULAR |
| In4 rate | 1.0× |
| All boid flocks | Initialised with random positions scattered within [−0.5, +0.5]² |
| CV positions (In1, In2) | (0.0, 0.0) until first ADC read |

---

## Out-of-Scope Interactions (v1)

- No MIDI control
- No preset save/load
- No parameter lock or mute per input
- No BPM tap-tempo for path rate
- Gate inputs do not affect path state in v1
