#include "lm/chip.hpp"
#include "lm/core.hpp"

#include <cstdio>

auto lm::chip::uart::port_count() -> uart_port { return 1; }
auto lm::chip::uart::max_port()   -> uart_port { return 0; }

auto lm::chip::uart::init(uart_port port, pin tx, pin rx, st baud_rate) -> void {}

auto lm::chip::uart::write(uart_port port, buf data) -> st
{
    // Route UART directly to stdout for the native console
    auto len = std::fwrite(data.data, 1, data.size, stdout);
    std::fflush(stdout);
    return len;
}
