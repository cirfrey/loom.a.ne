#include "lm/strands/blink.hpp"

#include "lm/core/types.hpp"
#include "lm/chip/gpio.hpp"
#include "lm/board.hpp"

lm::strands::blink::blink(fabric::strand_runtime_info& info)
{
    chip::gpio::init(board::status_led, chip::gpio::pin_mode::output);
}

auto lm::strands::blink::on_ready() -> fabric::managed_strand_status
{
    sw.click();
    return fabric::managed_strand_status::ok;
}

auto lm::strands::blink::before_sleep() -> fabric::managed_strand_status
{ return fabric::managed_strand_status::ok; }

auto lm::strands::blink::on_wake() -> fabric::managed_strand_status
{
    /// TODO: listen to blink events (set_pattern, send_oneoff).
    u32 dt = (u32)sw.click().last_segment<std::chrono::microseconds>();
    chip::gpio::set(board::status_led, bc.tick(dt));
    return fabric::managed_strand_status::ok;
}
