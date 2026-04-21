#pragma once

#include "lm/core/types.hpp"

namespace lm::ansi
{
    // Text Styles / Modifiers
    namespace style { enum style_t {
        none       = -1,

        reset      = 0,
        bold       = 1,
        dim        = 2,
        italic     = 3,
        underline  = 4,
        blink      = 5,
        invert     = 7,
        hidden     = 8,
        strike     = 9,
    }; }

    // Standard Foreground (Text) Colors
    namespace fg { enum fg_t {
        none    = -1,

        black   = 30,
        red     = 31,
        green   = 32,
        yellow  = 33,
        blue    = 34,
        magenta = 35,
        cyan    = 36,
        white   = 37,
        _default = 39,
        // Bright variants
        gray          = 90,
        bright_red    = 91,
        bright_green  = 92,
        bright_yellow = 93,
        bright_blue   = 94,
        bright_magenta= 95,
        bright_cyan   = 96,
        bright_white  = 97,
    }; }

    // Standard Background Colors
    namespace bg { enum bg_t {
        none     = -1,

        black    = 40,
        red      = 41,
        green    = 42,
        yellow   = 43,
        blue     = 44,
        magenta  = 45,
        cyan     = 46,
        white    = 47,
        _default = 49,
        // Bright variants
        gray          = 100,
        bright_red    = 101,
        bright_green  = 102,
        bright_yellow = 103,
        bright_blue   = 104,
        bright_magenta= 105,
        bright_cyan   = 106,
        bright_white  = 107,
    }; }

    namespace detail
    {
        // Sum up total size: \x1b + [ + (lengths of N) + (N-1 semicolons) + m
        template<int... Codes>
        static consteval auto total_size() -> st
        {
            auto num_len = [](int n) -> st {
                if (n < 0) return 0; // Skip "none"
                if (n == 0) return 1;
                st len = 0;
                for (int temp = n; temp > 0; temp /= 10) len++;
                return len;
            };

            st count = ((Codes != -1 ? 1 : 0) + ...);
            if (count == 0) return 0;

            // 2 (\x1b[) + lengths + (semicolons = count - 1) + 1 (m)
            return 2 + (num_len(Codes) + ...) + (count - 1) + 1;
        }

        template <int... Codes>
        struct code_t
        {
            static constexpr auto size = total_size<Codes...>();
            char data[size] = {0};

            constexpr code_t()
            {
                char* p = data;
                *p++ = '\x1b'; *p++ = '[';

                bool first = true;
                auto append = [&](int n) {
                    if (n == -1) return;
                    if (!first) *p++ = ';';
                    if (n >= 100) *p++ = (n / 100) + '0';
                    if (n >= 10)  *p++ = ((n / 10) % 10) + '0';
                    *p++ = (n % 10) + '0';
                    first = false;
                };

                (append(Codes), ...);

                *p++ = 'm';
            }
        };
    }
    template <int... Codes>
    inline constexpr auto code = detail::code_t<Codes...>{};

    // template<int... Codes>
    // inline constexpr auto code = text{
    //     code_raw<Codes...>.data,
    //     code_raw<Codes...>.size
    // };

    // TODO: runtime code as well.
    // template <int... Codes>
    // constexpr auto get_code(mut_text buf, auto... Codes) -> mut_text;
    // A simple runtime version that writes into a user-provided buffer
    // inline auto get_code_to_buf(char* buf, int s, int f, int b) -> st {
    //     char* p = buf;
    //     *p++ = '\x1b'; *p++ = '[';
    //     auto app = [&](int n) {
    //         if (n == -1) return;
    //         if (p != buf + 2) *p++ = ';';
    //         if (n >= 100) *p++ = (n / 100) + '0';
    //         if (n >= 10)  *p++ = ((n / 10) % 10) + '0';
    //         *p++ = (n % 10) + '0';
    //     };
    //     app(s); app(f); app(b);
    //     *p++ = 'm';
    //     return (st)(p - buf);
    // }
};
