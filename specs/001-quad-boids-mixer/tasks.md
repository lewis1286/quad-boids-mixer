# Tasks: Quadraphonic Boids Mixer

**Input**: Design documents from `specs/001-quad-boids-mixer/`  
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

**Tests**: Not explicitly requested — no test tasks generated. Hardware validation tasks serve as acceptance tests per the project constitution.

**Organization**: Tasks grouped by user story. Each story is independently buildable, flashable, and audibly verifiable on hardware.

---

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no shared dependency)
- **[Story]**: Which user story this task belongs to (US1–US4)
- Exact file paths included in every task

---

## Phase 1: Setup

**Purpose**: Establish the project skeleton that all subsequent tasks build on.

- [ ] T001 Create `src/` directory and `Makefile` using the libDaisy Daisy Patch template (`TARGET = circular_quadraphonic`, `SOURCES = src/main.cpp`)
- [ ] T002 Build the empty firmware with `make` and verify zero warnings and a valid `.elf` produced; record baseline `arm-none-eabi-size` output

**Checkpoint**: `make` succeeds cleanly — ready for implementation.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core types and utilities that every user story depends on. No user story work begins until this phase is complete.

**⚠️ CRITICAL**: All phase 3+ tasks depend on these.

- [ ] T003 [P] Implement `SpatialPosition` struct (float x, float y; both clamped to [−1, +1]) with `Clamp()` helper in `src/SpatialPos.h`
- [ ] T004 [P] Implement `Smoother` one-pole IIR class (templated `float`; `alpha` set at construction; `Process(target)` returns smoothed value) in `src/Smoother.h`
- [ ] T005 [P] Implement `QuadMixer` bilinear equal-power pan law in `src/QuadMixer.h`: `ComputeGains(SpatialPosition) → QuadGains`; `QuadGains` struct holds `fl, fr, rl, rr`; proof: `fl²+fr²+rl²+rr²=1`
- [ ] T006 Implement `PathBase` abstract interface (`virtual void Update(float dt) = 0; virtual SpatialPosition GetPosition() const = 0; virtual void SetRate(float rate) = 0`) in `src/PathBase.h`
- [ ] T007 Scaffold `src/main.cpp`: declare `DaisyPatch patch`; call `patch.Init()`, `patch.StartAdc()`, `patch.StartAudio(AudioCallback)` in `main()`; add empty `AudioCallback(InputBuffer, OutputBuffer, size_t)` stub; add empty `while(1)` loop

**Checkpoint**: Foundation complete — phase 3+ can proceed in parallel.

---

## Phase 3: User Story 1 — CV-Controlled Quadraphonic Panning (Priority: P1) 🎯 MVP

**Goal**: Audio Inputs 1 and 2 pan in real time via CV1–CV4. All four outputs contain the correct spatial mix. Inputs 3 and 4 default to field centre.

**Independent Test**: Patch a drone tone to In1, sweep CV1 with a slow LFO (0–3.3 V). FL and RL outputs should rise together while FR and RR fall, and vice versa — smooth, click-free, with no output on channels that should be silent at extreme positions.

- [ ] T008 [US1] Add `volatile float g_auto_pos_x[2] = {0,0}, g_auto_pos_y[2] = {0,0}` and encoder accumulators `volatile int32_t g_enc_delta = 0; volatile bool g_enc_pressed = false` as file-scope globals in `src/main.cpp`
- [ ] T009 [US1] Implement CV read and smooth in `AudioCallback` in `src/main.cpp`: call `patch.ProcessAnalogControls()`; map `GetKnobValue(CTRL_1..4)` from [0,1] to [−1,+1]; feed each through a `Smoother` (alpha ≈ 0.00131 for 10 Hz @ 48 kHz); store as `smooth_x1, smooth_y1, smooth_x2, smooth_y2`
- [ ] T010 [US1] Implement four-input mixing loop in `AudioCallback` in `src/main.cpp`: compute `QuadGains` for each of the four inputs (In1/In2 use smoothed CV; In3/In4 read `g_auto_pos_x/y`); for each sample write `out[ch][n] = Σ(in[i][n] × gains[i].ch) × 0.5f` for ch ∈ {FL, FR, RL, RR}
- [ ] T011 [US1] Hardware validation — patch audio source to In1, sweep CV1 and CV2 with LFOs, verify smooth pan across all four outputs; repeat for In2 with CV3/CV4; record `arm-none-eabi-size`

**Checkpoint**: CV-controlled quadraphonic panning fully functional and audibly verified on hardware.

---

## Phase 4: User Story 2 — Automated Path Movement (Priority: P2)

**Goal**: Inputs 3 and 4 each follow an independently-selectable geometric path (CIRCULAR or FIGURE8) animated in the main loop. Encoder cycles paths and adjusts rate.

**Independent Test**: Patch a constant tone to In3. Select CIRCULAR path. All four outputs should receive the signal in a repeating rotation sequence with no clicks. Encoder short press changes to FIGURE8 — movement pattern audibly changes.

- [ ] T012 [P] [US2] Implement `CircularPath : PathBase` in `src/CircularPath.h`: `float phase_ = 0.f, rate_ = 1.f`; `Update(dt)` increments `phase_ += rate_ × base_freq × dt × 2π`; `GetPosition()` returns `{cosf(phase_), sinf(phase_)}`; `SetRate(r)` clamps to [0.1, 4.0]
- [ ] T013 [P] [US2] Implement `Figure8Path : PathBase` in `src/Figure8Path.h`: same phase accumulator; `GetPosition()` returns `{sinf(phase_), sinf(2.f × phase_)}` (Lissajous 1:2 ratio)
- [ ] T014 [US2] Implement `PathSelector` in `src/PathSelector.h`: holds one instance each of `CircularPath`, `Figure8Path`, `BoidsPath` (forward-declared stub for now); `PathType` enum {CIRCULAR, FIGURE8, BOIDS}; `Cycle()` advances enum; `Active()` returns pointer to active `PathBase`
- [ ] T015 [US2] Implement `UIState` struct in `src/UIState.h`: `int selected_input = 3`; `PathSelector path3, path4`; `uint32_t enc_long_press_start_ms = 0`; `bool long_press_active = false`
- [ ] T016 [US2] Add 100 Hz path update throttle in main loop of `src/main.cpp`: compare `System::GetNow()` timestamp; call `ui.path3.Active()->Update(dt)` and `ui.path4.Active()->Update(dt)`; write `GetPosition()` results to `g_auto_pos_x/y[0]` and `g_auto_pos_y[1]`; add encoder accumulation for `patch.encoder` to `AudioCallback` (after `ProcessDigitalControls()`)
- [ ] T017 [US2] Add encoder UI handler in main loop of `src/main.cpp`: read `g_enc_delta` and reset; apply rotation to selected path rate (±0.05, clamped [0.1, 4.0]); detect short press (`g_enc_pressed`, total hold < 500 ms) → call `Cycle()` on selected path; detect long press (hold ≥ 500 ms) → toggle `ui.selected_input` between 3 and 4
- [ ] T018 [US2] Hardware validation — patch audio to In3 and In4; verify circular path rotates continuously through all four outputs; switch to figure-8 and verify crossing at centre; confirm encoder interaction produces no audio artefacts

**Checkpoint**: Automated path movement for Inputs 3 and 4 fully functional and audibly verified.

---

## Phase 5: User Story 3 — Boids-Based Organic Movement (Priority: P3)

**Goal**: A third path type (BOIDS) available on Inputs 3 and 4. Each runs an independent flock of N=32 virtual agents; flock centroid drives spatial position. Movement is non-repeating over multi-minute periods.

**Independent Test**: Select BOIDS path on In3. Listen for 60 seconds. Movement must be continuous, clearly non-repeating, and perceptibly different from circular/figure-8. No audio dropouts or OLED freezes.

- [ ] T019 [US3] Define `Boid` struct (`float pos_x, pos_y, vel_x, vel_y`) and `BoidsParams` struct with compile-time defaults (`w_sep=2.0, r_sep=0.12, w_align=1.0, r_align=0.35, w_coh=1.2, r_coh=0.35, max_velocity=0.07, max_force=0.03, centre_force=0.02, wander_force=0.02`) in `src/BoidsPath.h`; `N_BOIDS = 32`
- [ ] T020 [US3] Implement `BoidsPath::Update(float dt)` in `src/BoidsPath.h`: O(N²) neighbour loop computing separation, alignment, and cohesion steering forces per boid; clamp force magnitude to `max_force`; integrate velocity (`vel += steer`); clamp speed to `max_velocity`; integrate position (`pos += vel × dt × rate_`)
- [ ] T021 [US3] Add wander force (small random perturbation from `dsy_hal_seed_random()` or a simple LCG, magnitude `wander_force`) and soft centre-attraction force (`(0,0) − pos) × centre_force`) to each boid's acceleration in `BoidsPath::Update()` in `src/BoidsPath.h`; add boundary softening (negate velocity component if |pos| > 0.95)
- [ ] T022 [US3] Implement `BoidsPath::GetPosition()` returning mean centroid of all `N_BOIDS` positions; replace the `BoidsPath` forward-declaration stub in `src/PathSelector.h` with the full include; verify BOIDS type compiles and links
- [ ] T023 [US3] Hardware profiling — add `dsy_gpio_write(&debug_pin, 1/0)` GPIO toggle around the boids `Update()` call; measure pulse width with oscilloscope; if > 5 ms reduce `N_BOIDS` to 20 and re-profile; document measured duration and final `N_BOIDS` value in the PR description
- [ ] T024 [US3] Hardware validation — run BOIDS mode on both In3 and In4 simultaneously for 5 minutes; confirm non-repeating movement; confirm no audio dropout or OLED freeze; confirm all four audio outputs remain active

**Checkpoint**: Boids-based organic movement verified on hardware with profiling data recorded.

---

## Phase 6: User Story 4 — OLED Spatial Visualisation (Priority: P4)

**Goal**: OLED shows a real-time top-down quadraphonic field diagram with animated markers for all four inputs and a status bar showing path names and rates.

**Independent Test**: Sweep CV1 across its range while watching the OLED. The marker for In1 must move continuously and proportionally across the X axis of the field. Status bar must reflect the current path names and selected input.

- [ ] T025 [P] [US4] Implement `Display::RenderField(const SpatialPosition pos[4])` in `src/Display.h`: `patch.display.Fill(false)`; draw corner labels "FL", "FR", "RL", "RR" at their screen corners; for each of the four positions, convert to screen coords using `screen_x = (int)((p.x+1.f)*0.5f*119.f)+4`, `screen_y = (int)((1.f-(p.y+1.f)*0.5f)*43.f)+2`; clamp both to valid OLED range; draw a 3×3 filled square at that position using `DrawLine` (clamped coordinates mandatory — see research.md §3)
- [ ] T026 [P] [US4] Implement `Display::RenderStatus(const UIState& ui)` in `src/Display.h`: draw horizontal divider at y=48; write path abbreviation ("CIRC", "FIG8", "BOID") and rate ("×1.0") for In3 and In4 in the rows 49–63 status bar; highlight the currently selected input with `>` marker
- [ ] T027 [US4] Add 20 fps display throttle in main loop of `src/main.cpp`: collect current positions from smoothed CV values and `g_auto_pos_x/y`; call `Display::RenderField()`, `Display::RenderStatus()`, then `patch.display.Update()` every 50 ms
- [ ] T028 [US4] Hardware validation — sweep all CVs and switch paths; verify all four markers animate correctly on OLED; verify status bar updates match encoder interactions; confirm no firmware freeze from OLED rendering

**Checkpoint**: OLED visualisation complete and verified on hardware.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Build hygiene and constitution compliance checks before PR.

- [ ] T029 [P] Build with `-Wall -Wextra`; fix every compiler warning across all `src/` files (zero-warning policy per constitution)
- [ ] T030 [P] Run `clang-format` on all `src/*.h` and `src/*.cpp` files; commit the formatted versions
- [ ] T031 Record final `arm-none-eabi-size` output for `.elf`; confirm `text` (flash) < 90% of 128 KB internal budget and `data+bss` (SRAM) < 80% of 512 KB DTCM; include output in PR description
- [ ] T032 Complete all items in the hardware validation checklist in `specs/001-quad-boids-mixer/quickstart.md` (12 checkboxes); tick each and commit the updated file
- [ ] T033 Update `specs/001-quad-boids-mixer/quickstart.md` with the actual `N_BOIDS` value determined during profiling (T023), the measured boids update duration, and the final resource usage figures

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — **blocks all user stories**
- **US1 (Phase 3)**: Depends on Phase 2 only
- **US2 (Phase 4)**: Depends on Phase 2; integrates with Phase 3 output but US1 must be complete first (adds to the same `AudioCallback`)
- **US3 (Phase 5)**: Depends on Phase 4 (PathSelector must exist); BoidsPath plugs into PathSelector
- **US4 (Phase 6)**: Depends on Phase 3 (position values must be live); can overlap with Phase 4/5
- **Polish (Phase 7)**: Depends on all phases being feature-complete

### Within Each Phase

| Rule | Detail |
|------|--------|
| T003, T004, T005 | Parallel — different files, no shared dependency |
| T006 | Depends on T003 (uses SpatialPosition) |
| T007 | Depends on nothing except libDaisy being installed |
| T012, T013 | Parallel — different files |
| T025, T026 | Parallel — different functions, same header |
| T029, T030 | Parallel — different tools |

### User Story Dependencies

- **US1**: Can start after Foundational — no dependency on US2–US4
- **US2**: Requires US1 complete (adds paths to the same main.cpp and AudioCallback); then US3 and US4 can begin
- **US3**: Requires US2 complete (PathSelector must have BOIDS slot)
- **US4**: Requires US1 complete (needs live position data); can overlap with US2/US3

---

## Parallel Execution Examples

### Foundational phase (once Phase 1 done)

```
Task: T003 — SpatialPos.h
Task: T004 — Smoother.h          ← launch simultaneously
Task: T005 — QuadMixer.h
```

### Phase 4 path types

```
Task: T012 — CircularPath.h
Task: T013 — Figure8Path.h       ← launch simultaneously
```

### Phase 6 display functions

```
Task: T025 — Display::RenderField()
Task: T026 — Display::RenderStatus()   ← launch simultaneously
```

### Polish phase

```
Task: T029 — compiler warnings
Task: T030 — clang-format         ← launch simultaneously
```

---

## Implementation Strategy

### MVP First — User Story 1 Only (11 tasks)

1. T001–T002: Setup
2. T003–T007: Foundational
3. T008–T010: US1 implementation
4. T011: **Hardware validation — stop and verify before proceeding**

After T011: the module spatially pans two CV-controlled sources across four outputs. Fully useful and demonstrable.

### Incremental Delivery

| Milestone | Tasks | What you gain |
|-----------|-------|--------------|
| MVP | T001–T011 | CV quad panning for In1/In2 |
| + Paths | T012–T018 | Circular + figure-8 automation for In3/In4 |
| + Boids | T019–T024 | Organic flocking movement; profiling data |
| + Display | T025–T028 | Real-time OLED field diagram |
| + Polish | T029–T033 | PR-ready, constitution-compliant build |

---

## Notes

- `[P]` = different files, no dependency conflict — safe to implement in parallel
- `[USn]` label maps each task to its user story for traceability
- Hardware validation tasks (T011, T018, T024, T028) are acceptance gates — do not skip
- The boids profiling task (T023) is a **hard gate**: actual N_BOIDS may change based on measurement
- Coordinates fed to any `DrawLine` or text call MUST be clamped — violation causes a hard firmware freeze (see research.md §3)
- Commit after each hardware validation checkpoint, not just at the end of each phase
