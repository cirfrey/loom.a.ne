#pragma once

#include "lm/core/types.hpp"
#include "lm/fabric/types.hpp"

namespace lm::strands
{
    struct healthmon
    {
        healthmon(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~healthmon() = default;
    };
}
