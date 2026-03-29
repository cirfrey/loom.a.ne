#include "lm/tasks/app.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"

auto lm::app::init() -> void { lm::task::create(lm::config::task::app, lm::app::task); }
auto lm::app::task(lm::task::config const& cfg) -> void
{
    using tc = lm::task::event::task_command;
    auto tc_bus = tc::make_bus();
    tc::wait_for_start(tc_bus, cfg.id);

    while (!tc::should_stop(tc_bus, cfg.id))
    {
        lm::task::delay_ms(cfg.sleep_ms);
    }
}
