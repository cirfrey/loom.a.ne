#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::system
{
    auto init() -> void;

    [[noreturn]] auto reboot() -> void;
    [[noreturn]] auto panic(text, st code = 1) -> void;

    auto core_count() -> st;
}
