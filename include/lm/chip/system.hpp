#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::system
{
    auto init() -> void;

    // The code is used in native architectures since we need to exit
    // because we can't easily reboot.

    // On native, this probably just wraps to halt().
    [[noreturn]] auto reboot(st code = 0) -> void;
    [[noreturn]] auto panic(text, st code = 1) -> void;
    // On embedded, this will just stop the program on its tracks (ideally) until you forcefully reboot.
    // When unsupported, a deep sleep with no wakeup is close enough I guess.
    [[noreturn]] auto halt(st code = 0) -> void;

    auto core_count() -> st;
}
