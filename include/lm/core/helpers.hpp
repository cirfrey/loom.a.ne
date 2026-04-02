#pragma once

#include "lm/core/types.hpp"
#include "lm/core/veil.hpp"
#include "lm/core/cvt.hpp"

namespace lm
{
    // Automatically creates text from "literals" or strings.
    // The null terminator exists in the binary but is EXCLUDED from size.
    constexpr auto to_text = trans( [](auto&& str_or_lit) constexpr -> text {
        using T = veil::remove_ref_t<decltype(str_or_lit)>;

        if constexpr(veil::is_array_v<T>) {
            constexpr auto N = veil::extent_v<T>;
            return { .data = str_or_lit, .size = (N > 0 ? N - 1 : 0) };
        }
        else {
            if (!str_or_lit) return { nullptr, 0 };

            st len = 0;
            while (str_or_lit[len] != '\0') { ++len; }
            return { .data = str_or_lit, .size = len };
        }
    });
}
