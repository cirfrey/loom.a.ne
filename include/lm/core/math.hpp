#pragma once

#include "lm/core/types.hpp"

namespace lm
{
    constexpr auto clamp(auto v, auto lo, auto hi)
    { return v < lo ? lo : v > hi ? hi : v; }

    template <typename V>
    inline constexpr auto scale_unsafe(V value, st from_lo, st from_hi, st to_lo, st to_hi) -> V
    {
        // We use 64-bit intermediate to prevent any overflow during the math.
        // The compiler will optimize this away into 32-bit instructions if
        // the constants are small enough.
        return static_cast<V>(
            (static_cast<i64>(value) - from_lo) * (to_hi - to_lo) /
            (from_hi - from_lo) + to_hi
        );
    }

    template <typename V>
    inline constexpr auto scale(V value, st from_lo, st from_hi, st to_lo, st to_hi) -> V
    {
        return scale_unsafe(clamp(value, from_lo, from_hi), from_lo, from_hi, to_lo, to_hi);
    }

    template <u8 From, u8 To, typename V>
    inline constexpr auto scale_bits(V value) -> V
    {
        constexpr auto from_hi = (1ULL << From) - 1;
        constexpr auto to_hi   = (1ULL << To) - 1;
        return scale(value, 0, from_hi, 0, to_hi);
    }
}
