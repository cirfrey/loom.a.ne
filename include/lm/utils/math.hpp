#pragma once

namespace lm::math
{
    constexpr auto clamp(auto v, auto lo, auto hi)
    { return v < lo ? lo : v > hi ? hi : v; }
}
