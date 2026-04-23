#pragma once

#include "lm/fabric.hpp"

namespace lm::strands
{
    struct healthmon
    {
        using ri     = fabric::strand::strand_runtime_info;
        using status = fabric::strand::managed_strand_status;

        healthmon(ri& info);
        auto on_ready()     -> status;
        auto before_sleep() -> status;
        auto on_wake()      -> status;
        ~healthmon() = default;
    };
}
