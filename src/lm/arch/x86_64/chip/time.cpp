#include "lm/chip.hpp"
#include "lm/core.hpp"

#include <chrono>

auto lm::chip::time::uptime() -> u64
{
    static auto boot_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - boot_time).count();
}
