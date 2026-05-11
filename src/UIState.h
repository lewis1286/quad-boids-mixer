#pragma once

#include <stdint.h>

#include "PathSelector.h"

namespace qbm {

struct UIState {
    int          selected_input        = 3;     // 3 or 4
    PathSelector path3;
    PathSelector path4;
    uint32_t     enc_long_press_start_ms = 0;
    bool         long_press_active       = false;

    PathSelector& SelectedSelector() {
        return selected_input == 4 ? path4 : path3;
    }
};

}  // namespace qbm
