#include "lm/core/types.hpp"

#include "lm/chip/types.hpp"

namespace lm::chip::uart
{
    auto port_count() -> uart_port;
    auto max_port() -> uart_port;

    auto init(uart_port port, pin tx, pin rx, st baud_rate = 115200) -> void;
    auto write(uart_port port, view) -> void;
    /// TODO: read()
}
