#include "lm/tasks/app.hpp"

#include "lm/config.hpp"
#include "lm/fabric/types.hpp"

namespace lm::tasks
{
    struct app
    {
        app(fabric::task_runtime_info& info) {}
        auto on_ready() {}
        auto before_sleep() -> fabric::managed_task_status { return fabric::managed_task_status::ok; }
        auto on_wake()      -> fabric::managed_task_status { return fabric::managed_task_status::ok; }
        ~app() {}
    }
}
