// Use this as a starting point for your task.

#include "lm/tasks/example.hpp"

lm::tasks::example::example(fabric::task_runtime_info& info)
{
}

auto lm::tasks::example::on_ready() -> fabric::managed_task_status
{ 
    return fabric::managed_task_status::ok; 
}

auto lm::tasks::example::before_sleep() -> fabric::managed_task_status
{ 
    return fabric::managed_task_status::ok;
}

auto lm::tasks::example::on_wake() -> fabric::managed_task_status
{
    return fabric::managed_task_status::ok;
}

lm::tasks::example::~example()
{
}
