#pragma once

#include "lm/core/types.hpp"

namespace lm
{
    constexpr auto fnv1a_32(buf data) -> u32 {
        u32 hash = 0x811c9dc5;
        for (auto i = 0_st; i < data.size; ++i) {
            hash ^= (u32)(((u8*)data.data)[i]);
            hash *= 0x01000193;
        }
        return hash;
    }

    constexpr auto fnv1a_64(buf data) -> u64 {
        u64 hash = 0xcbf29ce484222325;
        for (auto i = 0_st; i < data.size; ++i) {
            hash ^= (u64)(((u8*)data.data)[i]);
            hash *= 0x100000001b3;
        }
        return hash;
    }

    inline namespace literals
    {
        constexpr auto operator ""_hash32(char const* str, st len) { return fnv1a_32({str, len}); }
        constexpr auto operator ""_hash64(char const* str, st len) { return fnv1a_64({str, len}); }
    }
}
