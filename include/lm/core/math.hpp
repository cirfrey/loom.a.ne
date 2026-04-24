#pragma once

#include "lm/core/types.hpp"

namespace lm
{
    constexpr auto clamp(auto v, auto lo, auto hi) {
        return
            v < static_cast<decltype(v)>(lo) ? lo :
            v > static_cast<decltype(v)>(hi) ? hi : v;
    }

    // Equivalent to arduino's map.
    template <typename V>
    constexpr auto scale_unsafe(V value, st from_lo, st from_hi, st to_lo, st to_hi) -> V
    {
        // We use 64-bit intermediate to prevent any overflow during the math.
        // The compiler will optimize this away into 32-bit instructions if
        // the constants are small enough.
        return static_cast<V>(
            (static_cast<i64>(value) - from_lo) * (to_hi - to_lo) /
            (from_hi - from_lo) + to_lo
        );
    }

    template <typename V>
    constexpr auto scale(V value, st from_lo, st from_hi, st to_lo, st to_hi) -> V
    {
        return scale_unsafe(clamp(value, from_lo, from_hi), from_lo, from_hi, to_lo, to_hi);
    }

    template <u8 bits>
    constexpr u64 unsigned_max = (bits == 64) ? ~0ULL : (1ULL << bits) - 1;
    template <u8 bits>
    constexpr i64 signed_max   = (1ULL << (bits - 1)) - 1;
    template <u8 bits>
    constexpr i64 signed_min   = -signed_max<bits> - 1;

    template <u8 bits_from, u8 bits_to, typename V>
    constexpr auto scale_bits(V value) -> V
    {
        constexpr auto from_hi = unsigned_max<bits_from>;
        constexpr auto to_hi   = unsigned_max<bits_to>;
        return scale(value, 0, from_hi, 0, to_hi);
    }

    template <st Bits>
    constexpr auto clip_bits_after(auto value) {
        // Create the mask at compile-time: (1 << 4) - 1 = 0x0F
        constexpr auto mask = (1ULL << Bits) - 1;
        return static_cast<decltype(value)>(value & mask);
    };
}
