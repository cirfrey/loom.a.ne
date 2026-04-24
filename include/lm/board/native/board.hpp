#pragma once

#include "lm/chip/types.hpp"

namespace lm::board
{
    enum pin : chip::pin
    {
        status_led,

        gpio_uart_trace_tx,
        gpio_uart_trace_rx,
    };

    enum uart_port : chip::uart_port
    {
        uart_zero  = 0,
        uart_trace = uart_zero,
        uart_unit_test = uart_trace,

        uart_port_count,
    };

}
