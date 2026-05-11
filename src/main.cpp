#include "daisy_patch.h"

#include "Display.h"
#include "QuadMixer.h"
#include "Smoother.h"
#include "SpatialPos.h"
#include "UIState.h"

using namespace daisy;
using namespace qbm;

// -----------------------------------------------------------------------------
// Hardware + shared state
// -----------------------------------------------------------------------------

static DaisyPatch patch;
static Display    display(patch);
static UIState    ui;

// Bridge from main loop (path/boids update) to AudioCallback (pan law).
// One-pole IIR smoothing in the callback masks any single-sample read tear.
static volatile float g_auto_pos_x[2] = { 0.0f, 0.0f };
static volatile float g_auto_pos_y[2] = { 0.0f, 0.0f };

// Mirror of the four smoothed positions, written by the callback after IIR
// processing. Main loop reads these for OLED rendering — keeps the display
// view and the audio view in sync, and avoids reading the non-volatile
// Smoother state across contexts.
static volatile float g_display_x[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static volatile float g_display_y[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

// Encoder is accumulated inside the audio callback to avoid the 1 ms-tick
// ghosting issue documented in research.md §3.
static volatile int32_t g_enc_delta   = 0;
static volatile bool    g_enc_pressed = false;
static volatile bool    g_enc_falling = false;

// IIR position smoothers, owned by the callback. alpha ≈ 0.00131 → ~10 Hz cutoff
// at 48 kHz, giving ~100 ms position lag — kills zipper noise without dulling
// expressive CV control.
constexpr float kSmoothAlpha = 0.00131f;
static Smoother<float> s_x1(kSmoothAlpha), s_y1(kSmoothAlpha);
static Smoother<float> s_x2(kSmoothAlpha), s_y2(kSmoothAlpha);
static Smoother<float> s_x3(kSmoothAlpha), s_y3(kSmoothAlpha);
static Smoother<float> s_x4(kSmoothAlpha), s_y4(kSmoothAlpha);

// -----------------------------------------------------------------------------
// Audio callback — interrupt context, must be heap-free and non-blocking.
// -----------------------------------------------------------------------------

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    patch.ProcessAnalogControls();
    patch.ProcessDigitalControls();

    // Encoder — accumulate here, drained in the main loop.
    g_enc_delta += patch.encoder.Increment();
    if (patch.encoder.RisingEdge())  g_enc_pressed = true;
    if (patch.encoder.FallingEdge()) g_enc_falling = true;

    // CV → field-space targets. CTRL_1..4 are knobs 0..3 on the Daisy Patch;
    // they correspond to the four CV inputs on Eurorack patch jacks.
    const float t_x1 = patch.GetKnobValue(DaisyPatch::CTRL_1) * 2.0f - 1.0f;
    const float t_y1 = patch.GetKnobValue(DaisyPatch::CTRL_2) * 2.0f - 1.0f;
    const float t_x2 = patch.GetKnobValue(DaisyPatch::CTRL_3) * 2.0f - 1.0f;
    const float t_y2 = patch.GetKnobValue(DaisyPatch::CTRL_4) * 2.0f - 1.0f;

    const float sx1 = s_x1.Process(t_x1);
    const float sy1 = s_y1.Process(t_y1);
    const float sx2 = s_x2.Process(t_x2);
    const float sy2 = s_y2.Process(t_y2);

    const float sx3 = s_x3.Process(g_auto_pos_x[0]);
    const float sy3 = s_y3.Process(g_auto_pos_y[0]);
    const float sx4 = s_x4.Process(g_auto_pos_x[1]);
    const float sy4 = s_y4.Process(g_auto_pos_y[1]);

    g_display_x[0] = sx1; g_display_y[0] = sy1;
    g_display_x[1] = sx2; g_display_y[1] = sy2;
    g_display_x[2] = sx3; g_display_y[2] = sy3;
    g_display_x[3] = sx4; g_display_y[3] = sy4;

    const QuadGains g1 = ComputeGains({ sx1, sy1 });
    const QuadGains g2 = ComputeGains({ sx2, sy2 });
    const QuadGains g3 = ComputeGains({ sx3, sy3 });
    const QuadGains g4 = ComputeGains({ sx4, sy4 });

    constexpr float kHeadroom = 0.5f;  // -6 dB to accommodate 4 inputs summed at a corner

    for (size_t n = 0; n < size; ++n) {
        const float i1 = in[0][n];
        const float i2 = in[1][n];
        const float i3 = in[2][n];
        const float i4 = in[3][n];

        out[0][n] = (i1 * g1.fl + i2 * g2.fl + i3 * g3.fl + i4 * g4.fl) * kHeadroom;
        out[1][n] = (i1 * g1.fr + i2 * g2.fr + i3 * g3.fr + i4 * g4.fr) * kHeadroom;
        out[2][n] = (i1 * g1.rl + i2 * g2.rl + i3 * g3.rl + i4 * g4.rl) * kHeadroom;
        out[3][n] = (i1 * g1.rr + i2 * g2.rr + i3 * g3.rr + i4 * g4.rr) * kHeadroom;
    }
}

// -----------------------------------------------------------------------------
// Encoder + path update + display — all run in the main loop.
// -----------------------------------------------------------------------------

static void HandleEncoder(uint32_t now_ms) {
    // Drain accumulated rotation atomically (single read of a 32-bit volatile).
    const int32_t delta = g_enc_delta;
    g_enc_delta = 0;

    if (delta != 0) {
        PathBase* active = ui.SelectedSelector().Active();
        active->SetRate(active->GetRate() + 0.05f * static_cast<float>(delta));
    }

    // Long-press detection: track press start, fire on threshold without waiting
    // for release so the user gets immediate haptic feedback at 500 ms.
    if (g_enc_pressed) {
        g_enc_pressed = false;
        ui.enc_long_press_start_ms = now_ms;
        ui.long_press_active       = true;
    }

    if (ui.long_press_active
        && (now_ms - ui.enc_long_press_start_ms) >= 500u
        && patch.encoder.Pressed())
    {
        ui.selected_input    = (ui.selected_input == 3) ? 4 : 3;
        ui.long_press_active = false;
        g_enc_falling        = false;  // swallow the release so it doesn't double-cycle
    }

    if (g_enc_falling) {
        g_enc_falling = false;
        if (ui.long_press_active) {
            // Released before 500 ms → short press → cycle path.
            ui.SelectedSelector().Cycle();
        }
        ui.long_press_active = false;
    }
}

int main(void) {
    patch.Init();

    // Seed each boids flock differently so In3 and In4 don't move identically.
    ui.path3.Boids().SetSeed(0xA5A5'A5A5u);
    ui.path4.Boids().SetSeed(0x5A5A'5A5Au);

    patch.StartAdc();
    patch.StartAudio(AudioCallback);

    uint32_t last_path_ms    = System::GetNow();
    uint32_t last_display_ms = System::GetNow();

    while (true) {
        const uint32_t now_ms = System::GetNow();

        HandleEncoder(now_ms);

        // Path update at 100 Hz — keeps boids CPU cost predictable.
        if ((now_ms - last_path_ms) >= 10u) {
            // Clamp dt to 20 ms so a slow display update (SPI Update() can
            // occasionally stall) does not produce a large boid position jump.
            uint32_t elapsed = now_ms - last_path_ms;
            if (elapsed > 20u) elapsed = 20u;
            const float dt = static_cast<float>(elapsed) * 0.001f;
            last_path_ms = now_ms;

            ui.path3.Active()->Update(dt);
            ui.path4.Active()->Update(dt);

            const SpatialPosition p3 = ui.path3.Active()->GetPosition();
            const SpatialPosition p4 = ui.path4.Active()->GetPosition();
            g_auto_pos_x[0] = p3.x;
            g_auto_pos_y[0] = p3.y;
            g_auto_pos_x[1] = p4.x;
            g_auto_pos_y[1] = p4.y;
        }

        // Display at 20 fps — Update() blocks on SPI, so we throttle it.
        if ((now_ms - last_display_ms) >= 50u) {
            last_display_ms = now_ms;

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
}
