#include "lm/chip.hpp"
#include "lm/core.hpp"

auto lm::chip::info::codename() -> text
{
    return "Bobby" | to_text;
}

auto lm::chip::info::name() -> text
{
    return "Loom Native Host Environment" | to_text;
}

auto lm::chip::info::uuid() -> text
{
    // Dummy MAC address for native host
    static char mac_str[] = "00:11:22:33:44:55";
    return { .data = mac_str, .size = sizeof(mac_str) - 1 };
}

auto lm::chip::info::banner() -> text
{
    return {nullptr, 0};
}
