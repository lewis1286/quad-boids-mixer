# Quickstart: Quadraphonic Boids Mixer

**Branch**: `001-quad-boids-mixer` | **Date**: 2026-05-10

---

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| `arm-none-eabi-g++` | ≥ 10.x | Part of the ARM GNU Toolchain |
| `arm-none-eabi-size` | same | For resource reporting (required by constitution) |
| `dfu-util` | ≥ 0.9 | For USB DFU flashing |
| `make` | ≥ 3.81 | GNU Make |
| libDaisy | current HEAD | Clone alongside this repo or set `LIBDAISY_DIR` |
| DaisySP | current HEAD | Optional; only needed if DaisySP DSP modules are used |

libDaisy build instructions: https://github.com/electro-smith/libDaisy

---

## Repository Layout

```
circular_quadraphonic/
├── Makefile
├── CLAUDE.md
├── src/
│   ├── main.cpp
│   ├── QuadMixer.h
│   ├── SpatialPos.h
│   ├── PathBase.h
│   ├── CircularPath.h
│   ├── Figure8Path.h
│   ├── BoidsPath.h
│   └── Display.h
└── specs/
    └── 001-quad-boids-mixer/
        ├── spec.md
        ├── plan.md
        ├── research.md
        ├── data-model.md
        ├── quickstart.md  ← this file
        └── contracts/
```

---

## Build

```bash
# From the project root:
make

# After a successful build, report resource usage (required before every PR):
arm-none-eabi-size build/circular_quadraphonic.elf
```

Expected output columns: `text` (flash), `data + bss` (SRAM). Flash must be < 90% of budget; SRAM < 80%.

---

## Flash

### USB DFU (recommended for development)

1. Hold the **BOOT** button on the Daisy Patch while connecting USB.
2. Release BOOT.
3. Run:

```bash
make program-dfu
```

### ST-Link (if DFU is unavailable)

```bash
make program
```

---

## Basic Patch — Minimal Test

This verifies the core CV panning of Inputs 1 and 2.

```
[Audio source A] ──→ AUDIO IN 1
[Audio source B] ──→ AUDIO IN 2
[LFO, slow ~0.1 Hz] ──→ CV IN 1   (X position for In1)
[LFO, slow ~0.1 Hz, offset 90°] ──→ CV IN 2   (Y position for In1)

AUDIO OUT 1 (FL) ──→ speaker or scope
AUDIO OUT 2 (FR) ──→ speaker or scope
AUDIO OUT 3 (RL) ──→ speaker or scope
AUDIO OUT 4 (RR) ──→ speaker or scope
```

Expected behaviour: Audio source A rotates continuously through the four output channels as the LFOs sweep. Source B is stationary at the field centre (no CV patched to CV3/CV4).

---

## Basic Patch — Automated Paths Test

```
[Audio source C] ──→ AUDIO IN 3
[Audio source D] ──→ AUDIO IN 4
(no CV connections needed)
```

Power on. OLED shows the field diagram with two markers animating along the default circular path. Encoder short press cycles In3 through CIRCULAR → FIGURE8 → BOIDS. Long press selects In4 for editing.

---

## Hardware Validation Checklist (required by constitution)

After each firmware build and before committing:

- [ ] Audio outputs present on all four channels with source patched to each input
- [ ] CV1 sweep from 0 V to 3.3 V moves In1 smoothly left → right (verify on scope or by ear)
- [ ] CV2 sweep from 0 V to 3.3 V moves In1 smoothly front → rear
- [ ] Circular path produces continuous smooth rotation audible across all four outputs
- [ ] Figure-8 path audibly crosses centre (equal presence FL+RR and FR+RL at crossings)
- [ ] Boids path produces non-repeating movement for > 60 seconds
- [ ] Encoder short press changes path without audible click or dropout
- [ ] Encoder long press switches selected input (status bar updates)
- [ ] OLED markers track signal positions correctly
- [ ] `arm-none-eabi-size` output recorded: flash < 90%, SRAM < 80%
- [ ] Zero `-Wall -Wextra` compiler warnings

---

## Profiling the Boids Simulation

Run the boids-only test build (see tasks.md for details) and measure main-loop iteration time using a GPIO toggle and oscilloscope:

```cpp
// Toggle a debug GPIO around the boids update block:
dsy_gpio_write(&debug_pin, 1);
boidsA.Update(dt);
boidsB.Update(dt);
dsy_gpio_write(&debug_pin, 0);
```

Target: the boids update should consume < 5 ms at N=32 per flock (i.e., < 50% of the 10 ms main-loop budget). If over budget, reduce N to 20 and re-profile.
