// Use this as a starting point for your strand.
#pragma once

#include "lm/fabric/types.hpp"

namespace lm::strands
{
    struct example
    {
        example(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~example();
    };
}
