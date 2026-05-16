// =============================================================================
// Display.h — OLED rendering helpers
//
// Two main render functions:
//   - RenderField:  the spatial top-down view with markers for each input
//   - RenderStatus: the bottom status bar showing path and rate
//
// All OLED draw calls go through libDaisy's `patch_.display`. The most
// important rule is COORDINATE CLAMPING — see research.md §3 for the gory
// detail of why passing a negative coord to DrawLine hard-freezes the firmware.
// We use private helper methods (DrawPixel, DrawHLine) that clamp internally
// so no caller can forget.
// =============================================================================
#pragma once

#include "daisy_patch.h"   // libDaisy hardware abstraction header

#include "PathBase.h"
#include "SpatialPos.h"
#include "UIState.h"

namespace qbm {

class Display {
public:
    // Constructor takes a REFERENCE to the DaisyPatch. We store the reference
    // so all our draw methods can call back into it. The patch must outlive
    // the Display object (it does — both are statics in main.cpp).
    //
    // `explicit` again: prevents `Display d = patch;` from being interpreted
    // as a Display construction (which would be confusing here).
    explicit Display(daisy::DaisyPatch& patch) : patch_(patch) {}

    // Renders the field area: corner labels + a 3x3 marker per input.
    // `const SpatialPosition pos[4]` is C-style "array of 4 SpatialPosition".
    // In practice C++ degrades this to a plain pointer; the `[4]` is only a
    // hint to the reader.
    void RenderField(const SpatialPosition pos[4]) {
        patch_.display.Fill(false);  // clear the framebuffer (all pixels off)

        // Corner labels. `SetCursor(x, y)` positions the text cursor; the next
        // `WriteString` draws there. Font_6x8 = 6 px wide, 8 px tall.
        patch_.display.SetCursor(2, 4);   patch_.display.WriteString("FL", Font_6x8, true);
        patch_.display.SetCursor(110, 4); patch_.display.WriteString("FR", Font_6x8, true);
        patch_.display.SetCursor(2, 38);  patch_.display.WriteString("RL", Font_6x8, true);
        patch_.display.SetCursor(110, 38);patch_.display.WriteString("RR", Font_6x8, true);

        // For each input, project its (x, y) field position onto OLED pixels
        // and draw a 3x3 dot there. The formulas come from contracts/ui-interactions.md.
        for (int i = 0; i < 4; ++i) {
            // (audio_x + 1) * 0.5 maps [-1, +1] -> [0, 1], times 119 spans the
            // usable horizontal width, plus 4 px padding on the left.
            int sx = static_cast<int>((pos[i].x + 1.0f) * 0.5f * 119.0f) + 4;
            // Y: +1 (rear) goes to BOTTOM of screen, -1 (front) to TOP.
            int sy = static_cast<int>((pos[i].y + 1.0f) * 0.5f * 43.0f) + 2;
            DrawMarker3x3(sx, sy);
        }
    }

    // Status bar at the bottom. Two rows — In3 on top, In4 below.
    void RenderStatus(const UIState& ui) {
        // Horizontal divider at y=48. Status bar is rows 49-63 (14 px),
        // two stacked 7-px text rows so 14-char labels fit at 6 px/char width.
        DrawHLine(0, 127, 47);

        DrawLabel(0, 48, ui, 3);
        DrawLabel(0, 56, ui, 4);
    }

    // Push the framebuffer to the physical OLED over SPI. This can take a
    // few milliseconds, so we throttle calls to ~20 fps in the main loop.
    void Update() { patch_.display.Update(); }

private:
    // `static` here means "class-scope static function" — no `this` pointer,
    // basically a free function namespaced inside the class. Equivalent to a
    // Python `@staticmethod`. Used for stateless helpers like clamping.
    static int ClampX(int v) { return v < 0 ? 0 : (v > 127 ? 127 : v); }
    static int ClampY(int v) { return v < 0 ? 0 : (v > 63  ? 63  : v); }

    // libDaisy has no DrawPixel — we abuse DrawLine with the same start/end
    // point. Wrapping it here means every pixel write is clamped automatically.
    void DrawPixel(int x, int y) {
        const int cx = ClampX(x);
        const int cy = ClampY(y);
        patch_.display.DrawLine(cx, cy, cx, cy, true);
    }

    void DrawHLine(int x0, int x1, int y) {
        patch_.display.DrawLine(ClampX(x0), ClampY(y), ClampX(x1), ClampY(y), true);
    }

    // 3x3 filled square centred on (cx, cy). 9 single-pixel draws —
    // unsophisticated but trivially correct and well within our render budget.
    void DrawMarker3x3(int cx, int cy) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                DrawPixel(cx + dx, cy + dy);
            }
        }
    }

    // Renders one status-bar row: "[ |>]In3 CIRC x1.0"
    void DrawLabel(int x, int y, const UIState& ui, int which) {
        // Ternary on which selector to look at. `&` makes `sel` a reference
        // — no copying.
        const PathSelector& sel = (which == 4) ? ui.path4 : ui.path3;
        const bool selected = (ui.selected_input == which);

        // STACK-ALLOCATED string buffer. C/C++ has no built-in dynamic string
        // type (well, std::string exists, but it heap-allocates and we banned
        // that). A char array is the standard heap-free alternative.
        char buf[24];
        const char* name = PathName(sel.Type());
        const float rate = sel.Active()->GetRate();

        // `sizeof(buf)` is a COMPILE-TIME-CONSTANT byte count. For
        // `char buf[24]` it's 24. (sizeof a pointer would be 4 or 8 — not
        // what you want here. Since `buf` is an actual array, sizeof works.)
        FormatLine(buf, sizeof(buf), selected, which, name, rate);

        patch_.display.SetCursor(ClampX(x), ClampY(y));
        patch_.display.WriteString(buf, Font_6x8, true);
    }

    // Map enum to a short string. `const char*` is a pointer to read-only
    // characters; equivalent to a Python str literal.
    static const char* PathName(PathType t) {
        switch (t) {
            case PathType::CIRCULAR: return "CIRC";
            case PathType::FIGURE8:  return "FIG8";
            case PathType::BOIDS:    return "BOID";
            case PathType::STATIC:   return "OFF";
        }
        return "????";
    }

    // Heap-free formatter — does the job of `sprintf` for our specific layout
    // without dragging in printf's huge code size.
    //
    // `char* buf` = pointer to caller's char array. `unsigned int n` = buf
    // capacity. We write characters until we'd overrun, then null-terminate.
    static void FormatLine(char* buf, unsigned int n, bool selected, int which,
                           const char* name, float rate) {
        unsigned int i = 0;

        // LAMBDA — anonymous inline function. `[&]` is the CAPTURE LIST: the
        // `&` means "capture by reference any variables I refer to". Inside
        // the lambda we can read/write `i`, `n`, `buf` from the surrounding
        // scope. The lambda takes one `char` argument and returns void.
        // Python analogue: a closure, like `lambda c: ...`.
        auto put = [&](char c) { if (i + 1u < n) buf[i++] = c; };

        // Layout: ">In3 CIRC x1.0" or " In4 BOID x4.0"
        put(selected ? '>' : ' ');
        put('I'); put('n');
        put('0' + which);  // ASCII trick: '0'+3 = '3'. Works because digit
                           // characters are consecutive in ASCII.
        put(' ');
        // C-string traversal: `name` is a pointer; `name[k]` reads the k-th
        // char; `'\0'` (zero byte) is the string terminator. The loop exits
        // when we hit it.
        for (unsigned int k = 0; name[k] && i + 1u < n; ++k) put(name[k]);
        put(' '); put('x');

        // Format the rate as "DD.D" — two-digit integer part, one-digit fraction.
        const int whole  = static_cast<int>(rate);                          // truncates
        const int tenths = static_cast<int>(rate * 10.0f) - whole * 10;
        put('0' + (whole / 10) % 10);
        put('0' + whole % 10);
        put('.');
        put('0' + (tenths < 0 ? -tenths : tenths) % 10);

        // Null-terminate. C strings are arrays ending in '\0'; without it,
        // WriteString would walk past the end and print garbage.
        if (i < n) buf[i] = '\0';
        else buf[n - 1] = '\0';
    }

    // Reference member — must be initialised in the constructor's init list
    // (you can't assign to it later, you can only bind it once at construction).
    daisy::DaisyPatch& patch_;
};

}  // namespace qbm
