
#include "lm/chip.hpp"
#include "lm/core.hpp"

namespace lm::chip::gpio {
    // Mock registers to hold the state of up to 40 "pins"
    static bool pin_states[40] = {false};
}

auto lm::chip::gpio::init(pin p, pin_mode mode) -> void
{
    // Just bounds check on native to prevent segfaults
    if (p < 40) pin_states[p] = false;
}

auto lm::chip::gpio::get(pin p) -> bool
{
    return (p < 40) ? pin_states[p] : false;
}

auto lm::chip::gpio::set(pin p, bool level) -> void
{
    if (p < 40) pin_states[p] = level;
}

auto lm::chip::gpio::attach_interrupt(pin p, interrupt_edge edge, isr_handler handler, void* arg) -> void
{
    // On native, hardware interrupts don't exist.
    // You would typically trigger these manually via a test harness or an OS signal.
}

auto lm::chip::gpio::detach_interrupt(pin p) -> void {}
