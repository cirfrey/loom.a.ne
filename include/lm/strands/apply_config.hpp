#pragma once

#include "lm/fabric/types.hpp"

namespace lm::strands
{
    struct apply_config
    {
        apply_config(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~apply_config();
    };
}
