# Feature Specification: Quadraphonic Boids Mixer

**Feature Branch**: `001-quad-boids-mixer`  
**Created**: 2026-05-10  
**Status**: Draft  
**Input**: User description: "This project will be a quadrophonic mixer for the Daisy Patch system with an additional boids algorithm. The CV inputs will control x and y positions for two incoming audio signals (Audio in 1 mix controlled by CV 1 (x) and CV2 (y), and Audio in 2 mix controlled by CV 3(x) and CV4 (y). the additional two audio inputs will be mixed according to internally defined paths stored on daisy. Two of the paths will be circular and figure 8, other paths may be added later."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - CV-Controlled Quadraphonic Panning (Priority: P1)

A musician patches two audio sources into Audio Inputs 1 and 2, and routes CV signals into CV1–CV4. As the CVs change (from an LFO, sequencer, or joystick), both signals move independently through the four-channel sound field in real time. Each of the four output channels reflects the evolving spatial mix.

**Why this priority**: This is the core interactive function of the module. It validates the fundamental CV-to-position mapping and the four-channel mixing engine. The module delivers direct musical value with only these two inputs and four CVs.

**Independent Test**: Can be fully tested by patching a drone tone to Input 1 and driving CV1–CV2 with a slow LFO, then confirming each output level rises and falls smoothly and correctly as position sweeps across the full field.

**Acceptance Scenarios**:

1. **Given** Audio In 1 is active and CV1 sweeps from minimum to maximum, **When** the CV rises, **Then** the signal pans smoothly from the left pair (FL, RL) to the right pair (FR, RR) with no audible clicks or level discontinuities.
2. **Given** Audio In 2 is active and CV3–CV4 are held at mid-range, **When** the module powers up, **Then** Audio In 2 appears at equal level across all four outputs.
3. **Given** both inputs active and their CVs at opposite corners, **When** the module runs, **Then** each input is isolated to its respective corner channel with no cross-bleed.
4. **Given** a CV input exceeds the nominal ADC range, **When** the module processes the signal, **Then** the position clamps to the field boundary with no distortion, fault, or audio artefact.

---

### User Story 2 - Automated Path Movement for Inputs 3 and 4 (Priority: P2)

A musician patches audio sources into Inputs 3 and 4. Using the encoder, they select a movement path (circular or figure-8). The signals begin moving continuously through the quadraphonic field following the chosen trajectory, creating an evolving spatial effect without any external CV.

**Why this priority**: This differentiates the module from a standard quadraphonic panner and enables standalone spatial animation, making the module useful even in minimal patch configurations.

**Independent Test**: Can be fully tested by patching a constant tone into Input 3, selecting the circular path with the encoder, and listening for continuous smooth rotation across FL → FR → RR → RL → FL.

**Acceptance Scenarios**:

1. **Given** Audio In 3 is active and the circular path is selected, **When** the module runs, **Then** the signal rotates continuously and evenly through all four corners with no discontinuities.
2. **Given** Audio In 4 is active and the figure-8 path is selected, **When** the module runs, **Then** the signal traces a figure-8 trajectory, crossing the centre of the field on each pass.
3. **Given** a path is running, **When** the musician presses the encoder to switch to a different path, **Then** the new path is adopted smoothly without audible clicks or jumps.
4. **Given** no audio source is patched to Input 3, **When** a path is active, **Then** the output channels for that signal remain silent.

---

### User Story 3 - Boids-Based Organic Movement (Priority: P3)

A musician selects boids mode as the path for Input 3 or Input 4. A flock of virtual autonomous agents (boids) runs as an internal simulation; the aggregate or representative positions of these agents are mapped to spatial positions in the quadraphonic field and used to drive the panning of the assigned audio input. The boids simulation is independent of the audio signals themselves — it runs as a self-contained spatial animation layer. The resulting movement is organic and non-repetitive, adding a living quality that geometric paths cannot provide.

**Computational note**: The boids simulation must remain within the Daisy Patch's processing budget alongside the four-channel mixing engine, CV processing, OLED rendering, and any active geometric paths. Boid count and update rate must be tuned to leave sufficient headroom; the implementation plan must include a profiling step.

**Why this priority**: Adds musical distinctiveness and long-form variation; distinguishes the module from purely geometric path options.

**Independent Test**: Can be tested by patching a signal to the boids-assigned input, selecting boids mode, and verifying that movement is continuous and non-repeating over a 5-minute window, and perceptually distinct from circular and figure-8 paths.

**Acceptance Scenarios**:

1. **Given** boids mode is selected for an input, **When** the module runs for 60 seconds, **Then** the spatial movement does not exactly retrace any previous 10-second window.
2. **Given** the boids simulation is running alongside all four audio inputs active, **When** the module runs under full load, **Then** there are no audio dropouts, glitches, or OLED freezes attributable to boids computational overhead.
3. **Given** boids mode is active and virtual agents cluster at one region of the field, **When** separation rules engage, **Then** the governed audio input's position shifts smoothly away from the cluster without abrupt jumps.

---

### User Story 4 - OLED Spatial Visualisation (Priority: P4)

The musician glances at the OLED during a performance and immediately reads the current position of all four audio signals on a top-down diagram of the quadraphonic field.

**Why this priority**: Essential for usability during live performance — provides visual confirmation of signal positions without requiring measurement tools or patch inspection.

**Independent Test**: Can be tested by sweeping CV1 across its range and confirming the corresponding marker on the OLED moves continuously across the X axis of the display.

**Acceptance Scenarios**:

1. **Given** all four inputs are active, **When** the module is running, **Then** the OLED shows a 2D field diagram with four corner labels (FL, FR, RL, RR) and four distinct moving markers.
2. **Given** CV1 sweeps from minimum to maximum, **When** viewed on the OLED, **Then** the marker for Audio In 1 moves continuously and proportionally across the X axis.
3. **Given** an automated path is active for In 3 or 4, **When** viewed on the OLED, **Then** the corresponding marker animates along the selected trajectory.

---

### Edge Cases

- What happens when two or more inputs share the exact same spatial position? (Signals should sum at those outputs; the combined level must not exceed the Eurorack output range.)
- What happens if all four outputs are driven simultaneously at maximum level? (Soft-clip or headroom margin must prevent output clipping.)
- What happens during power-on before CVs are patched? (Module starts with safe default positions — e.g., all inputs centred.)
- What happens when the encoder is turned rapidly through all path options? (Path selection must debounce; rapid switching must not produce audio artefacts.)
- What happens if path movement rate is set to zero? (Signal holds at its current position without oscillation or noise artefacts.)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The module MUST accept four independent mono audio inputs and produce four independent mono audio outputs representing front-left (FL), front-right (FR), rear-left (RL), and rear-right (RR) spatial positions.
- **FR-002**: Audio Input 1 spatial position MUST be controlled in real time by CV1 (X axis) and CV2 (Y axis).
- **FR-003**: Audio Input 2 spatial position MUST be controlled in real time by CV3 (X axis) and CV4 (Y axis).
- **FR-004**: Audio Inputs 3 and 4 MUST follow selectable internally-defined movement paths without requiring external CV.
- **FR-005**: At minimum, circular and figure-8 paths MUST be available at first release; the path system MUST be extensible to allow additional paths in future firmware versions without restructuring the core mixing engine.
- **FR-006**: Audio Inputs 3 and 4 MUST each have an independently selectable path; the musician selects the path for In 3 and the path for In 4 separately via the encoder (e.g., In 3 = circular, In 4 = boids simultaneously).
- **FR-007**: The boids simulation MUST run as an independent spatial animation layer: a flock of virtual agents moves according to standard boids rules (separation, alignment, cohesion); the resulting aggregate or representative agent position(s) are mapped to the spatial positions used to pan the assigned audio input(s). The simulation MUST NOT consume audio-path CPU cycles that would cause dropouts.
- **FR-008**: The musician MUST be able to select the active path(s) for automated inputs using the module's encoder.
- **FR-009**: The full ADC input range on CV1–CV4 MUST map to the full spatial extent of the quadraphonic field on both X and Y axes.
- **FR-010**: All spatial position transitions MUST be parameter-smoothed to eliminate audible clicks or zipper noise.
- **FR-011**: The OLED MUST display a real-time top-down representation of the quadraphonic field showing the current position of all four signals simultaneously.
- **FR-012**: All audio processing MUST operate within real-time constraints: no heap allocation, no blocking calls, and no mutex locks inside the audio callback.

### Key Entities

- **AudioInput** (1–4): a mono audio signal entering the module, characterised by its current spatial position (X, Y).
- **SpatialPosition**: a 2D coordinate (X, Y) in the normalised quadraphonic field (−1.0 to +1.0 on each axis), mapped to four-channel amplitude gains via a pan law.
- **MovementPath**: a parameterised trajectory function that produces a SpatialPosition as a function of time (instances: circular, figure-8, boids).
- **QuadChannel**: one of the four output positions — FL, FR, RL, RR — each associated with a corner of the quadraphonic field.
- **PanLaw**: the algorithm converting a SpatialPosition into per-channel gain values; equal-power panning assumed.
- **Boid**: an autonomous agent with position and velocity used in the flocking simulation; its position maps directly to a SpatialPosition for the governed audio input.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All four audio outputs contain the correctly spatially-panned mix for all four inputs with no distortion at nominal Eurorack signal levels (±5V audio range).
- **SC-002**: CV-driven position changes for Inputs 1 and 2 are reflected in the audio mix within a single audio block (≤ 1 ms at 48 kHz).
- **SC-003**: Automated path movement for Inputs 3 and 4 produces smooth, continuous spatial movement with no audible stepping, clicking, or discontinuities at any point along the trajectory.
- **SC-004**: Path selection via the encoder registers and takes effect within 100 ms of the physical interaction.
- **SC-005**: The OLED display updates at a minimum of 10 frames per second during active movement, maintaining visual continuity.
- **SC-006**: Boids mode produces movement that does not exactly retrace any 10-second window over a 5-minute observation period.
- **SC-007**: Flash and SRAM usage remain within the hardware budget defined in the project constitution (flash < 90% of available, SRAM < 80%).
- **SC-008**: Switching between path modes produces no audible artefacts (clicks, pops, or noise injected into any output channel).

## Assumptions

- The quadraphonic field is modelled as a unit square; positions are mapped to four corner channels using an equal-power pan law.
- Audio Inputs 3 and 4 move at a fixed internal rate in v1; no BPM synchronisation is required for this release.
- The encoder button cycles through available paths; encoder rotation adjusts the movement speed/rate of the active path.
- CV inputs spanning 0–3.3V (Daisy Patch ADC range) map to the full normalised field extent (−1.0 to +1.0) on the corresponding axis.
- Signals from all four inputs contribute additively to each output channel; a headroom margin or soft-clip stage prevents output clipping when multiple inputs converge.
- The OLED shows a top-down schematic: four corner labels with small animated markers representing each signal's current position.
- Persistent storage of path preferences across power cycles is out of scope for v1.
- MIDI, USB, and BPM-sync are out of scope for this feature.
- The boids simulation governs at minimum the positions of Inputs 3 and 4 (exact scope pending clarification).
- The module will be validated exclusively on physical Daisy Patch hardware as required by the project constitution.
