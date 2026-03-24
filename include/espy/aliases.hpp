// Who likes long types?
#pragma once

#include <stdint.h>

namespace espy
{
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;
    using f32 = float;
    using f64 = double;
    static_assert(sizeof(f32) == 32/8 && sizeof(f64) == 64/8);

    inline namespace literals
    {
        constexpr auto operator""_u8(unsigned long long val)  { return static_cast<u8>(val); }
        constexpr auto operator""_u16(unsigned long long val) { return static_cast<u16>(val); }
        constexpr auto operator""_u32(unsigned long long val) { return static_cast<u32>(val); }
        constexpr auto operator""_u64(unsigned long long val) { return static_cast<u64>(val); }

        constexpr auto operator""_i8(unsigned long long val)  { return static_cast<i8>(val); }
        constexpr auto operator""_i16(unsigned long long val) { return static_cast<i16>(val); }
        constexpr auto operator""_i32(unsigned long long val) { return static_cast<i32>(val); }
        constexpr auto operator""_i64(unsigned long long val) { return static_cast<i64>(val); }
    };
}
