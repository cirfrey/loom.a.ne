// Use this as a starting point for your task.
// Don't forget to get a unique id and config in lm::config::task.
#include "lm/tasks/example.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"

auto lm::example::init() -> void { lm::task::create(lm::config::task::example, lm::example::task); }
auto lm::example::task(lm::task::config const& cfg) -> void
{
    using tc = lm::task::event::task_command;
    auto tc_bus = tc::make_bus();
    tc::wait_for_start(tc_bus, cfg.id);

    while (!tc::should_stop(tc_bus, cfg.id))
    {
        lm::task::sleep_ms(cfg.sleep_ms);
    }
}
