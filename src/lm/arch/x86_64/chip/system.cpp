#include "lm/chip.hpp"
#include "lm/core.hpp"

#include <thread>

auto lm::chip::system::init() -> void
{
    // No-op for native OS
}

auto lm::chip::system::reboot(st code) -> void
{
    halt(code);
}

auto lm::chip::system::panic(text msg, st code) -> void
{
    std::printf("%.*s", (int)msg.size, msg.data);
    halt(code);
}

auto lm::chip::system::halt(st code) -> void
{
    std::exit(code);
}

auto lm::chip::system::core_count() -> st
{
    st cores = std::thread::hardware_concurrency();
    return cores > 0 ? cores : 1;
}
