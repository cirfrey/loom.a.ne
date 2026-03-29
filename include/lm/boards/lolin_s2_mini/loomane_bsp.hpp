#pragma once

// Already imported, this file is 'pasted' at the end of board.hpp.
// We'll include it anyway to avoid editor squiggles.
#include "lm/board.hpp"

namespace lm::board::inline bsp
{
    enum pin : pin_t {
        status_led = 15
    };
}
