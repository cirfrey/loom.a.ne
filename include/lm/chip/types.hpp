#pragma once

#include "lm/core/types.hpp"

namespace lm::chip
{
    using pin       = st;
    using uart_port = st;
    using socket_t  = i32;

    constexpr auto invalid_pin       = (pin)-1;
    constexpr auto invalid_uart_port = (uart_port)-1;
    constexpr auto invalid_socket    = (socket_t)-1;
}
