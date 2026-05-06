<!--
SYNC IMPACT REPORT
==================
Version change: (none) → 1.0.0
Modified principles: N/A (initial ratification)
Added sections:
  - Core Principles (I–V)
  - Hardware Constraints
  - Development Workflow
  - Governance
Templates requiring updates:
  - .specify/templates/plan-template.md  ✅ Constitution Check gates aligned
  - .specify/templates/spec-template.md  ✅ No mandatory section conflicts
  - .specify/templates/tasks-template.md ✅ Task categories reflect embedded DSP workflow
Deferred TODOs: none
-->

# Circular Quadraphonic Constitution

## Core Principles

### I. Real-Time Audio Safety (NON-NEGOTIABLE)

The audio callback MUST run without heap allocation, blocking calls, mutex locks,
or system calls. All memory MUST be allocated statically or on the stack before the
audio callback starts. Any violation of this principle risks audio glitches, dropped
samples, or hard faults on the STM32H750.

**Rules**:
- `new` / `delete` / `malloc` / `free` are PROHIBITED inside `AudioCallback`.
- No `std::vector`, `std::string`, or other heap-owning containers in the audio path.
- All DSP state MUST be initialized in `main()` or `Init()` before audio starts.
- Shared state between audio and non-audio contexts MUST use atomic flags or
  double-buffering, never a mutex that can block.

### II. Hardware-First Validation

All features MUST be tested on physical Daisy Patch hardware before being considered
complete. Simulation and unit tests are useful for logic verification, but cannot
substitute for on-hardware validation of audio quality, CV accuracy, and timing.

**Rules**:
- Every PR MUST document the hardware test performed (patch description, signal path).
- CV input/output accuracy MUST be verified with a multimeter or oscilloscope.
- Flash size and RAM usage MUST be reported after each build (`arm-none-eabi-size`).

### III. Eurorack Signal Standards

All CV and gate signals MUST conform to Eurorack electrical conventions. Deviations
MUST be explicitly justified and documented in the relevant spec.

**Rules**:
- CV outputs MUST operate within ±5V.
- 1V/octave pitch tracking MUST be calibrated and validated across at least 4 octaves.
- Gate thresholds: HIGH ≥ 2.5V, LOW ≤ 0.5V.
- Audio I/O MUST match Daisy Patch's ±5V audio range without clipping under nominal use.

### IV. Deterministic Signal Flow

The signal routing and parameter mapping MUST be explicit, traceable, and free of
hidden state mutations. Every audio block's output MUST be fully determined by its
inputs and the current parameter state — no surprise coupling between voices or
channels.

**Rules**:
- Each audio processing stage MUST have a clearly named function or class.
- Parameter smoothing MUST be applied consistently — raw knob values MUST NOT
  directly modulate audio-rate parameters.
- Quadraphonic channel assignment MUST be documented in a signal-flow diagram
  or ASCII art in the relevant spec.

### V. Simplicity and Constraint-Awareness

The firmware MUST remain within hardware resource budgets. Complexity MUST be
justified by a concrete user need. YAGNI applies strictly.

**Rules**:
- Flash usage MUST stay below 90% of available flash (128KB reserved for bootloader).
- DTCM/SRAM usage MUST stay below 80% to allow stack headroom.
- No abstraction layer may be introduced without a concrete, immediate use case.
- When two implementations meet the same requirement, PREFER the simpler one.

## Hardware Constraints

**Target hardware**: Electrosmith Daisy Patch (STM32H750, ARM Cortex-M7 @ 480 MHz)

| Resource       | Budget                        |
|----------------|-------------------------------|
| Flash          | 128 KB (qspi: 8 MB available) |
| SRAM (DTCM)    | 512 KB                        |
| SDRAM          | 64 MB (for audio buffers)     |
| Audio I/O      | 4 in / 4 out, 48 kHz, 32-bit  |
| CV inputs      | 4× ADC (12-bit, 0–3.3V)       |
| CV outputs     | 2× DAC (12-bit, 0–3.3V)       |
| Gate/trigger   | 2× in, 2× out                 |
| OLED           | 128×64 SSD1309                |
| Encoder        | 1× with push button           |

**Toolchain**: `arm-none-eabi-g++`, libDaisy, DaisySP, `make` build system.

## Development Workflow

1. **Branch**: create a feature branch via `/speckit-git-feature` before any work.
2. **Specify**: write or update the feature spec via `/speckit-specify`.
3. **Plan**: generate an implementation plan via `/speckit-plan`.
4. **Build**: `make` from project root; fix all warnings before committing.
5. **Flash**: `make program-dfu` or `make program` (ST-Link) to test on hardware.
6. **Validate**: perform hardware test documented in the spec; record results.
7. **Commit**: commit only after hardware validation passes.

**Build hygiene**:
- Zero compiler warnings policy: `-Wall -Wextra` MUST produce no warnings.
- `clang-format` MUST be applied to all changed `.cpp`/`.h` files before commit.
- Resource report (`arm-none-eabi-size`) MUST be reviewed before each PR.

## Governance

This constitution supersedes all ad-hoc practices. Amendments require:
1. A written rationale explaining why the change is necessary.
2. Version increment per semantic versioning (MAJOR/MINOR/PATCH — see below).
3. Propagation of any changes to `.specify/templates/` and `CLAUDE.md`.

**Versioning policy**:
- MAJOR: Removal or fundamental redefinition of a principle.
- MINOR: New principle or section added; material expansion of existing guidance.
- PATCH: Clarifications, wording fixes, non-semantic refinements.

All PRs and reviews MUST verify compliance with the Core Principles.
Complexity violations MUST be documented in the plan's Complexity Tracking table.

**Version**: 1.0.0 | **Ratified**: 2026-05-06 | **Last Amended**: 2026-05-06
