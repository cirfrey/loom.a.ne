#include "lm/chip/all.hpp"
#include "lm/core/all.hpp"

#include <cstdio>

auto lm::chip::uart::port_count() -> uart_port { return 1; }
auto lm::chip::uart::max_port()   -> uart_port { return 0; }

auto lm::chip::uart::init(
    [[maybe_unused]] uart_port port,
    [[maybe_unused]] pin tx,
    [[maybe_unused]] pin rx,
    [[maybe_unused]] st baud_rate
) -> void {}

auto lm::chip::uart::write([[maybe_unused]] uart_port port, buf data) -> st
{
    // Route UART directly to stdout for the native console
    auto len = std::fwrite(data.data, 1, data.size, stdout);
    std::fflush(stdout);
    return len;
}
