// =============================================================================
// UIState.h — the encoder + path-selection state container
//
// Everything mutable that the UI logic touches lives in here, so main.cpp
// only needs to pass around a single `UIState& ui` reference rather than
// 5 or 6 separate values.
// =============================================================================
#pragma once

#include <stdint.h>

#include "PathSelector.h"

namespace qbm {

// A plain struct holding UI state. All members public — this is essentially
// a record, not an encapsulated class.
struct UIState {
    int          selected_input        = 3;  // which input (3 or 4) the
                                             // encoder is currently editing
    PathSelector path3;                      // path selector for audio In3
    PathSelector path4;                      // path selector for audio In4
    uint32_t     enc_long_press_start_ms = 0;
    bool         long_press_active       = false;

    // Inline method — returns a REFERENCE to whichever PathSelector is
    // currently being edited. `&` in the return type means "reference",
    // so callers can mutate the returned selector and the change persists.
    PathSelector& SelectedSelector() {
        return selected_input == 4 ? path4 : path3;
    }
};

}  // namespace qbm
