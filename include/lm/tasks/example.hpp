// Use this as a starting point for your task.
#pragma once

namespace lm::tasks
{
    struct example
    {
        example(fabric::task_runtime_info& info);
        auto on_ready()     -> fabric::managed_task_status;
        auto before_sleep() -> fabric::managed_task_status;
        auto on_wake()      -> fabric::managed_task_status;
        ~example() {}
    };
}
