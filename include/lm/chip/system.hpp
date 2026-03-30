#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::system
{
    auto init() -> void;
    auto reboot() -> void;
    auto core_count() -> st;
}
