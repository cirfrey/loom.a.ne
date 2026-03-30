#pragma once

#include "lm/chip/types.hpp"

namespace lm::chip::gpio
{
    // The pin mapping (0 is LED, 1 is whatever, etc...) are configured in
    // the bsp for the specific board. See lm/boards/.../bsp.hpp
    enum class pin_mode : u8 {
        input,
        input_pullup,
        input_pulldown,
        output,
    };
    auto init(pin p, pin_mode) -> void;
    auto get(pin p) -> bool; /// TODO: what about analog values?
    auto set(pin p, bool level) -> void;

    enum class interrupt_edge : u8 {
        rising,
        falling,
        any,
    };
    using isr_handler = void (*)(void* arg);
    auto attach_interrupt(pin p, interrupt_edge edge, isr_handler handler, void* arg = nullptr) -> void;
    auto detach_interrupt(pin p) -> void;
}
