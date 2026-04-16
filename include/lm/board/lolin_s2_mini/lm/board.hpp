#pragma once

#include "lm/chip/types.hpp"

namespace lm::board
{
    enum pin : chip::pin
    {
        status_led = 15,

        gpio_uart_trace_tx = 17,
        gpio_uart_trace_rx = 18,
    };

    enum uart_port : chip::uart_port
    {
        uart_zero = 0,
        uart_one  = 1,
        uart_two  = 2,

        /// TODO: this really doesn't belong here but for now it is convenient.
        uart_internal = uart_zero,
        uart_trace    = uart_one,
        uart_midi     = uart_two,

        uart_port_count,
    };

    enum rhport : u8
    {
        rhport_phy = 0,
        rhport_usbip = 1,
    };
}
