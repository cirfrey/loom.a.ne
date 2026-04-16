#pragma once

#include "lm/core/types.hpp"
#include "lm/port.hpp"

namespace lm
{
    constexpr u16 hton16(lm::u16 v)
    {
        if constexpr(lm::port::is_little_endian){
            return (v >> 8) | (v << 8);
        } else {
            return v;
        }
    }

    constexpr u32 hton32(lm::u32 v)
    {
        if constexpr(lm::port::is_little_endian){
            return ((v & 0xFF000000u) >> 24) | ((v & 0x00FF0000u) >>  8)
            | ((v & 0x0000FF00u) <<  8) | ((v & 0x000000FFu) << 24);
        } else {
            return v;
        }
    }

    constexpr u16 ntoh16(lm::u16 v) { return hton16(v); }
    constexpr u32 ntoh32(lm::u32 v) { return hton32(v); }
}
