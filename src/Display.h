#pragma once

#include "daisy_patch.h"

#include "PathBase.h"
#include "SpatialPos.h"
#include "UIState.h"

namespace qbm {

class Display {
public:
    explicit Display(daisy::DaisyPatch& patch) : patch_(patch) {}

    // Draws field corners and a 3x3 filled marker per input position.
    // Coordinates passed to DrawLine MUST be clamped — see research.md §3.
    void RenderField(const SpatialPosition pos[4]) {
        patch_.display.Fill(false);

        patch_.display.SetCursor(2, 4);   patch_.display.WriteString("RL", Font_6x8, true);
        patch_.display.SetCursor(110, 4); patch_.display.WriteString("RR", Font_6x8, true);
        patch_.display.SetCursor(2, 38);  patch_.display.WriteString("FL", Font_6x8, true);
        patch_.display.SetCursor(110, 38);patch_.display.WriteString("FR", Font_6x8, true);

        for (int i = 0; i < 4; ++i) {
            int sx = static_cast<int>((pos[i].x + 1.0f) * 0.5f * 119.0f) + 4;
            int sy = static_cast<int>((1.0f - (pos[i].y + 1.0f) * 0.5f) * 43.0f) + 2;
            DrawMarker3x3(sx, sy);
        }
    }

    void RenderStatus(const UIState& ui) {
        // Horizontal divider at y=48. Status bar is rows 49-63 (14 px),
        // two stacked 7-px text rows so 14-char labels fit at 6 px/char width.
        DrawHLine(0, 127, 48);

        DrawLabel(0, 49, ui, 3);
        DrawLabel(0, 57, ui, 4);
    }

    void Update() { patch_.display.Update(); }

private:
    static int ClampX(int v) { return v < 0 ? 0 : (v > 127 ? 127 : v); }
    static int ClampY(int v) { return v < 0 ? 0 : (v > 63  ? 63  : v); }

    void DrawPixel(int x, int y) {
        const int cx = ClampX(x);
        const int cy = ClampY(y);
        patch_.display.DrawLine(cx, cy, cx, cy, true);
    }

    void DrawHLine(int x0, int x1, int y) {
        patch_.display.DrawLine(ClampX(x0), ClampY(y), ClampX(x1), ClampY(y), true);
    }

    void DrawMarker3x3(int cx, int cy) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                DrawPixel(cx + dx, cy + dy);
            }
        }
    }

    void DrawLabel(int x, int y, const UIState& ui, int which) {
        const PathSelector& sel = (which == 4) ? ui.path4 : ui.path3;
        const bool selected = (ui.selected_input == which);

        char buf[24];
        const char* name = PathName(sel.Type());
        const float rate = sel.Active()->GetRate();

        FormatLine(buf, sizeof(buf), selected, which, name, rate);

        patch_.display.SetCursor(ClampX(x), ClampY(y));
        patch_.display.WriteString(buf, Font_6x8, true);
    }

    static const char* PathName(PathType t) {
        switch (t) {
            case PathType::CIRCULAR: return "CIRC";
            case PathType::FIGURE8:  return "FIG8";
            case PathType::BOIDS:    return "BOID";
        }
        return "????";
    }

    static void FormatLine(char* buf, unsigned int n, bool selected, int which,
                           const char* name, float rate) {
        // Hand-rolled, heap-free formatting. Layout: ">In3 CIRC x1.0"
        unsigned int i = 0;
        auto put = [&](char c) { if (i + 1u < n) buf[i++] = c; };
        put(selected ? '>' : ' ');
        put('I'); put('n'); put('0' + which); put(' ');
        for (unsigned int k = 0; name[k] && i + 1u < n; ++k) put(name[k]);
        put(' '); put('x');
        const int whole  = static_cast<int>(rate);
        const int tenths = static_cast<int>(rate * 10.0f) - whole * 10;
        put('0' + (whole / 10) % 10);
        put('0' + whole % 10);
        put('.');
        put('0' + (tenths < 0 ? -tenths : tenths) % 10);
        if (i < n) buf[i] = '\0';
        else buf[n - 1] = '\0';
    }

    daisy::DaisyPatch& patch_;
};

}  // namespace qbm
