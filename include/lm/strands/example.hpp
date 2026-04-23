// Use this as a starting point for your strand.
#pragma once

#include "lm/fabric.hpp"

namespace lm::strands
{
    struct example
    {
        using ri     = fabric::strand::strand_runtime_info;
        using status = fabric::strand::managed_strand_status;

        example(ri& info);
        auto on_ready()     -> status;
        auto before_sleep() -> status;
        auto on_wake()      -> status;
        ~example();

        ri& info;
    };
}
