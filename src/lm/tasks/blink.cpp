#include "lm/tasks/blink.hpp"

#include "lm/core/types.hpp"
#include "lm/chip/gpio.hpp"
#include "lm/board.hpp"


lm::tasks::blink::blink(fabric::task_runtime_info& info) {}

auto lm::tasks::blink::on_ready() -> fabric::managed_task_status
{ return fabric::managed_task_status::ok; }

auto lm::tasks::blink::before_sleep() -> fabric::managed_task_status
{ return fabric::managed_task_status::ok; }

auto lm::tasks::blink::on_wake() -> fabric::managed_task_status
{
    /// TODO: listen to blink events (set_pattern, send_oneoff).
    u32 dt = (u32)sw.click().last_segment<std::chrono::microseconds>();
    chip::gpio::set(board::status_led, bc.tick(dt));
    return fabric::managed_task_status::ok;
}
