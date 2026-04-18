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
    // TODO: Add a little randomness at the end so the mesh won't
    //       trip up if it has multiple different native looms on it.
    static char uuid_str[] = "loom.a.ne:x86_64";
    return { .data = uuid_str, .size = sizeof(uuid_str) - 1 };
}

auto lm::chip::info::banner() -> text
{
    return {nullptr, 0};
}
