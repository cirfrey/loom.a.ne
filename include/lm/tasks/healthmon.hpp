#pragma once

#include "lm/core/types.hpp"
#include "lm/fabric/types.hpp"

namespace lm::tasks
{
    struct healthmon
    {
        healthmon(fabric::task_runtime_info& info);
        auto on_ready()     -> fabric::managed_task_status;
        auto before_sleep() -> fabric::managed_task_status;
        auto on_wake()      -> fabric::managed_task_status;
        ~healthmon() {}
    };
}
