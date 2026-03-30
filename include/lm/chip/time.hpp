#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::time
{
    // In microseconds.
    auto uptime() -> u64;
}
