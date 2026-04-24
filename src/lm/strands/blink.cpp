#include "lm/strands/blink.hpp"

#include "lm/core/types.hpp"
#include "lm/chip/gpio.hpp"
#include "lm/board.hpp"

lm::strands::blink::blink([[maybe_unused]] ri& info)
{
    chip::gpio::init(board::status_led, chip::gpio::pin_mode::output);
}

auto lm::strands::blink::on_ready() -> status
{
    sw.click();
    return status::ok;
}

auto lm::strands::blink::on_wake() -> status
{
    /// TODO: listen to blink events (set_pattern, send_oneoff).
    u32 dt = (u32)sw.click().last_segment<std::chrono::microseconds>();
    chip::gpio::set(board::status_led, bc.tick(dt));
    return status::ok;
}
