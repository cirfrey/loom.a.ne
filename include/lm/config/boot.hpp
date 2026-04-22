// This is obviously only supposed to be used during the boot sequence.
// If you use it and modify the config while strands are running you will cause issues,
// lm::config is lockless by design and you will cause a race condition.
#pragma once

#include "lm/config.hpp"

namespace lm
{
    extern config_t& config_rw;
}
