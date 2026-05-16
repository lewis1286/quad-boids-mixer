// =============================================================================
// main.cpp — top-level firmware: wiring + the two concurrent contexts
//
// In embedded audio firmware there are essentially TWO concurrent "threads",
// even though we never spawn a thread explicitly:
//
//   1. AudioCallback (INTERRUPT context). libDaisy invokes this every time
//      the audio hardware finishes one buffer (every ~0.67 ms at 48 kHz with
//      32-sample blocks). It MUST be fast and must NOT block — if it stalls,
//      the audio output glitches.
//
//   2. main()'s while(true) loop (NORMAL context). Runs whenever the audio
//      callback isn't running, i.e. the rest of the CPU's time.
//
// Communication between the two goes through `volatile` globals — see below.
// =============================================================================

#include "daisy_patch.h"

#include "Display.h"
#include "QuadMixer.h"
#include "Smoother.h"
#include "SpatialPos.h"
#include "UIState.h"

// `using namespace X;` is the C++ equivalent of Python's `from X import *`.
// Brings every name from the namespace into the current scope so we can
// write `DaisyPatch` instead of `daisy::DaisyPatch`. At file scope in a
// single-TU main.cpp this is fine; in headers it's considered bad style
// because it leaks into every file that includes the header.
using namespace daisy;
using namespace qbm;

// -----------------------------------------------------------------------------
// Hardware + shared state
// -----------------------------------------------------------------------------

// `static` at file scope means INTERNAL LINKAGE — the variable is not visible
// to other .cpp files (we only have one .cpp file here, but the habit is
// good). The default constructor runs automatically before main() — this is
// called "static initialisation".
static DaisyPatch patch;
static Display    display(patch);  // Display ctor binds its reference to `patch`
static UIState    ui;

// `volatile` is the key word for INTERRUPT/MAIN-LOOP communication. It tells
// the compiler "this memory can change unexpectedly — don't cache it in a
// register or skip reads/writes you think are redundant". Without it, the
// compiler might decide that since main() never writes `g_auto_pos_x`, it
// can hoist the read out of the loop — and then never see updates from the
// audio callback.
//
// `volatile` is NOT the same as Python's `threading.Lock`. It does not
// provide atomicity across multiple words. For 32-bit values on ARM Cortex-M7
// a single read/write IS atomic at the hardware level, so a single
// volatile float is safe to share.
//
// Bridge from main loop (path/boids update) -> AudioCallback (pan law).
static volatile float g_auto_pos_x[2] = { 0.0f, 0.0f };
static volatile float g_auto_pos_y[2] = { 0.0f, 0.0f };

// Mirror of the four smoothed positions, written by the callback after IIR
// processing. Main loop reads these for OLED rendering.
static volatile float g_display_x[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static volatile float g_display_y[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Encoder events are produced in the callback, consumed in the main loop.
// `int32_t` = "exactly 32-bit signed integer" — fixed-width type from
// <stdint.h>. Use these in embedded code so the size is unambiguous.
static volatile int32_t g_enc_delta   = 0;   // accumulated rotation since main loop last drained it
static volatile bool    g_enc_pressed = false;
static volatile bool    g_enc_falling = false;

// One-pole IIR smoothers for the four input positions. alpha = 0.00131
// gives a ~10 Hz cutoff at 48 kHz sample rate, i.e. ~100 ms time constant —
// fast enough to feel responsive, slow enough to wipe out zipper noise.
// Block rate is ~1500 Hz (48 kHz / 32 samples). Time constant τ ≈ 1/(α × 1500).
// 0.00131 → ~510 ms (very sluggish). 0.05 → ~13 ms (snappy, still zipper-free).
constexpr float kSmoothAlpha = 0.05f;
static Smoother<float> s_x1(kSmoothAlpha), s_y1(kSmoothAlpha);
static Smoother<float> s_x2(kSmoothAlpha), s_y2(kSmoothAlpha);
static Smoother<float> s_x3(kSmoothAlpha), s_y3(kSmoothAlpha);
static Smoother<float> s_x4(kSmoothAlpha), s_y4(kSmoothAlpha);

// -----------------------------------------------------------------------------
// Audio callback — INTERRUPT context, must be heap-free and non-blocking.
//
// libDaisy invokes this once per audio buffer. `size` is typically 32 or 48
// samples; `in` / `out` are arrays-of-channels (per the libDaisy convention,
// NOT interleaved samples). At 48 kHz with size=48, we have ~1 ms wall-clock
// per call — and we must finish in < ~700 us to leave headroom.
// -----------------------------------------------------------------------------

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    // Refresh ADC samples (knobs/CV) and digital inputs (encoder).
    patch.ProcessAnalogControls();
    patch.ProcessDigitalControls();

    // Encoder — accumulate rotation and capture press/release events.
    // The += and assignment to volatile globals make these visible to the
    // main loop on its next read. Why here and not in the main loop?
    // libDaisy's encoder driver has a 1 ms debounce tick — if you read
    // Increment() multiple times within a tick from the main loop, you
    // get duplicates. Reading exactly once per audio callback is the
    // canonical pattern; see research.md §3.
    g_enc_delta += patch.encoder.Increment();
    if (patch.encoder.RisingEdge())  g_enc_pressed = true;
    if (patch.encoder.FallingEdge()) g_enc_falling = true;

    // Read the four knobs/CVs. GetKnobValue returns [0, 1]; we map that to
    // the field range [-1, +1] by `* 2 - 1`.
    const float t_x1 = patch.GetKnobValue(DaisyPatch::CTRL_1) * 2.0f - 1.0f;
    const float t_y1 = patch.GetKnobValue(DaisyPatch::CTRL_2) * 2.0f - 1.0f;
    const float t_x2 = patch.GetKnobValue(DaisyPatch::CTRL_3) * 2.0f - 1.0f;
    const float t_y2 = patch.GetKnobValue(DaisyPatch::CTRL_4) * 2.0f - 1.0f;

    // Smooth all four input positions. For In1/In2 we smooth the live CV
    // values; for In3/In4 we smooth the path-generated positions that the
    // main loop wrote to g_auto_pos_*. Once smoothed, both are equivalent.
    const float sx1 = s_x1.Process(t_x1);
    const float sy1 = s_y1.Process(t_y1);
    const float sx2 = s_x2.Process(t_x2);
    const float sy2 = s_y2.Process(t_y2);

    const float sx3 = s_x3.Process(g_auto_pos_x[0]);
    const float sy3 = s_y3.Process(g_auto_pos_y[0]);
    const float sx4 = s_x4.Process(g_auto_pos_x[1]);
    const float sy4 = s_y4.Process(g_auto_pos_y[1]);

    // Publish the smoothed values so the OLED can show the same positions
    // the audio engine is actually using. Without this, the display would
    // show the raw CV / raw path positions, which can lead/lag the audio.
    g_display_x[0] = sx1; g_display_y[0] = sy1;
    g_display_x[1] = sx2; g_display_y[1] = sy2;
    g_display_x[2] = sx3; g_display_y[2] = sy3;
    g_display_x[3] = sx4; g_display_y[3] = sy4;

    // Compute the four quad gains for each input. The pan law is O(1) per
    // input, so this is 4 inputs * ~6 sqrtf calls = negligible cost.
    const QuadGains g1 = ComputeGains({ sx1, sy1 });
    const QuadGains g2 = ComputeGains({ sx2, sy2 });
    const QuadGains g3 = ComputeGains({ sx3, sy3 });
    const QuadGains g4 = ComputeGains({ sx4, sy4 });

    // -6 dB master headroom. Four inputs panned to the same corner sum to
    // 4x at that corner's output; without headroom, we'd clip.
    constexpr float kHeadroom = 0.5f;

    // The actual mix loop — one iteration per sample in the buffer. We sum
    // each input's contribution to each output corner, scaled by that
    // input's gain into that corner. 16 multiply-adds per sample, ~768 per
    // 48-sample block — well under the time budget.
    for (size_t n = 0; n < size; ++n) {
        const float i1 = in[0][n];
        const float i2 = in[1][n];
        const float i3 = in[2][n];
        const float i4 = in[3][n];

        out[0][n] = (i1 * g1.fl + i2 * g2.fl + i3 * g3.fl + i4 * g4.fl) * kHeadroom;  // FL
        out[1][n] = (i1 * g1.fr + i2 * g2.fr + i3 * g3.fr + i4 * g4.fr) * kHeadroom;  // FR
        out[2][n] = (i1 * g1.rl + i2 * g2.rl + i3 * g3.rl + i4 * g4.rl) * kHeadroom;  // RL
        out[3][n] = (i1 * g1.rr + i2 * g2.rr + i3 * g3.rr + i4 * g4.rr) * kHeadroom;  // RR
    }
}

// -----------------------------------------------------------------------------
// Encoder handler — runs in main loop, drains events posted by the callback
// -----------------------------------------------------------------------------

static void HandleEncoder(uint32_t now_ms) {
    // Drain accumulated rotation. Read-then-zero is technically a race with
    // the callback (it could fire between the read and the write and lose
    // an increment), but losing one click out of dozens is imperceptible
    // and the alternative is heavier (atomics or interrupt disabling).
    const int32_t delta = g_enc_delta;
    g_enc_delta = 0;

    if (delta != 0) {
        PathBase* active = ui.SelectedSelector().Active();
        active->SetRate(active->GetRate() + 0.05f * static_cast<float>(delta));
    }

    // On press, record start time and arm the long-press detector.
    if (g_enc_pressed) {
        g_enc_pressed = false;
        ui.enc_long_press_start_ms = now_ms;
        ui.long_press_active       = true;
    }

    // Long press: if button has been held for >= 500 ms, toggle which
    // input we're editing. Fires immediately at the threshold (without
    // waiting for release) so the user gets prompt feedback.
    if (ui.long_press_active
        && (now_ms - ui.enc_long_press_start_ms) >= 500u
        && patch.encoder.Pressed())
    {
        ui.selected_input    = (ui.selected_input == 3) ? 4 : 3;
        ui.long_press_active = false;
        // Swallow the upcoming release so it isn't interpreted as a short press.
        g_enc_falling = false;
    }

    // Release before 500 ms = short press = cycle path type.
    if (g_enc_falling) {
        g_enc_falling = false;
        if (ui.long_press_active) {
            ui.SelectedSelector().Cycle();
        }
        ui.long_press_active = false;
    }
}

// -----------------------------------------------------------------------------
// main() — the entry point. After Init/StartAudio, runs the main loop forever.
//
// This is THE only entry point of the firmware. Embedded systems have no
// shell, no command line — the bootloader jumps directly to main() after
// hardware initialisation.
// -----------------------------------------------------------------------------

int main(void) {
    patch.Init();  // libDaisy hardware init: pin muxing, clocks, peripherals...

    // Give the two boids flocks distinct starting seeds so they don't move
    // identically. The numbers are arbitrary "looks random" hex.
    ui.path3.Boids().SetSeed(0xA5A5A5A5u);
    ui.path4.Boids().SetSeed(0x5A5A5A5Au);

    patch.StartAdc();                  // begin ADC sampling
    patch.StartAudio(AudioCallback);   // begin audio — callback now firing

    // From here on, AudioCallback runs in the background (interrupt context).
    // Our main loop handles the slower-rate work: UI, path updates, display.

    uint32_t last_path_ms    = System::GetNow();  // ms since boot
    uint32_t last_display_ms = System::GetNow();

    // `while (true)` — the classic embedded forever loop. Returning from
    // main() on a microcontroller is meaningless; there's nothing to return
    // to. Most firmware never exits.
    while (true) {
        const uint32_t now_ms = System::GetNow();

        HandleEncoder(now_ms);

        // Path update at 100 Hz (every 10 ms). Throttling here keeps the
        // boids O(N^2) loop predictable.
        if ((now_ms - last_path_ms) >= 10u) {
            // Defensive: clamp the elapsed time. If the display Update()
            // stalled on SPI for 30 ms, the boids would otherwise integrate
            // a huge dt and visibly teleport. Capping at 20 ms keeps motion
            // bounded.
            uint32_t elapsed = now_ms - last_path_ms;
            if (elapsed > 20u) elapsed = 20u;
            const float dt = static_cast<float>(elapsed) * 0.001f;  // ms -> seconds
            last_path_ms = now_ms;

            ui.path3.Active()->Update(dt);
            ui.path4.Active()->Update(dt);

            // Read the new positions and publish them to the audio callback
            // via the volatile globals.
            const SpatialPosition p3 = ui.path3.Active()->GetPosition();
            const SpatialPosition p4 = ui.path4.Active()->GetPosition();
            g_auto_pos_x[0] = p3.x;
            g_auto_pos_y[0] = p3.y;
            g_auto_pos_x[1] = p4.x;
            g_auto_pos_y[1] = p4.y;
        }

        // OLED render at 20 fps. patch.display.Update() blocks while it
        // pushes bytes over SPI, so we throttle to avoid eating CPU.
        if ((now_ms - last_display_ms) >= 50u) {
            last_display_ms = now_ms;

            // Snapshot the four smoothed positions for rendering.
            // Reading volatiles implicitly converts to non-volatile floats
            // in the struct field — each subscript is one volatile load.
            const SpatialPosition positions[4] = {
                { g_display_x[0], g_display_y[0] },
                { g_display_x[1], g_display_y[1] },
                { g_display_x[2], g_display_y[2] },
                { g_display_x[3], g_display_y[3] },
            };
            display.RenderField(positions);
            display.RenderStatus(ui);
            display.Update();
        }
    }

    // Unreachable. Some compilers warn if main has no return; this satisfies them.
    // The C++ standard actually says main without a return is equivalent to
    // `return 0;`, but explicit is clearer for non-trivial main functions.
    // return 0;
}
