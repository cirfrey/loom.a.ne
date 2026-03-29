#pragma once

#include "lm/aliases.hpp"

#include "lm/task/types.hpp"


namespace lm::board::inline general
{
    auto reboot() -> void;
    auto init_hardware() -> void;
    // In microseconds.
    auto get_uptime() -> u64;
}

namespace lm::board::inline misc
{
    // Most likely what's going to be used in the boot banner.
    auto get_pretty_name() -> char const*;
    auto get_mac_str() -> char const*;

    struct banner_art_t { char const* data; u32 len; };
    // Custom banner art per-chip for funsies.
    // There's a default one, if you don't want to customize it
    // just set this to .data=nullptr, .len=0.
    auto get_banner_art() -> banner_art_t;
}

namespace lm::board::inline console
{
    auto init_console(u32 baud_rate = 115200) -> void;
    auto console_write(void const* data, u32 len) -> void;
}

namespace lm::board::inline mem
{
    // How much RAM there is in total in bytes.
    auto get_ram() -> u32;
    // Available RAM (heap) in bytes.
    auto get_free_ram() -> u32;
    // Get the maxiumum amount of RAM that's been used (high water mark) in bytes.
    auto get_peak_ram() -> u32;
    // Get the size of the largest free block in RAM in bytes.
    auto get_largest_free_ram_block() -> u32;
}

namespace lm::board::inline gpio
{
    // The pin mapping (0 is LED, 1 is whatever, etc...) are configured in
    // the bsp for the specific board. See lm/boards/.../bsp.hpp
    using pin_t = u8;
    enum class pin_mode : u8 {
        input,
        input_pullup,
        input_pulldown,
        output,
    };
    // Callback type for interrupts
    auto init_pin(pin_t p, pin_mode) -> void;
    auto get_pin(pin_t p) -> bool; /// TODO: bool for now.
    auto set_pin(pin_t p, bool level) -> void;

    enum class interrupt_edge : u8 {
        rising,
        falling,
        any,
    };
    using isr_handler_t = void (*)(void* arg);
    auto attach_interrupt(pin_t p, interrupt_edge edge, isr_handler_t handler, void* arg = nullptr) -> void;
    auto detach_interrupt(pin_t p) -> void;
}

namespace lm::board::inline cpu
{
    // Translates logical affinity to physical core ID for this specific board.
    // Used in lm::task::create.
    auto get_physical_core(lm::task::affinity affinity) -> int;
    auto get_core_count() -> u8;
    // In celcius. Expect a NAN on unsupported hardware.
    auto get_cpu_temperature() -> f32;
}


#if !defined(LOOMANE_NO_INCLUDE_BSP)
    #include "loomane_bsp.hpp"
#endif
