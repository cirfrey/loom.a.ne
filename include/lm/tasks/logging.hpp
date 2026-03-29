/// TODO: cleanup this header.
#pragma once

#include "lm/aliases.hpp"

#include <source_location>

namespace lm::logging
{
    struct fmt_with_sourceloc {
        const char* fmt;
        std::source_location loc;
        constexpr fmt_with_sourceloc(const char* f, std::source_location l = std::source_location::current()) : fmt(f), loc(l) {}
    };

    // Inits uart at pin 17 & 18 and forwards
    // - ESP_LOG* messages to uart.
    // - logf or lograw uart (with diagnostic for dropped logs due to ringbuf overflow)
    auto init_uart() -> void;
    auto logf_uart(const char* fmt, ...) -> int;
    auto lograw_uart(const char* bytes, u32 len) -> void;

    constexpr auto _rawfilename(char const* file) -> char const*
    {
        auto end = file;
        while(*++end != '\0');
        while(*end != '\\' && *end != '/') --end;
        return end + 1;
    }

    // A trace is a log you only see in uart.
    // Mostly used for debugging.
    // If you wanna show it to CDC or HID use logt instead.
    #define LOOM_TRACE(fmt, ...)  lm::logging::logf_uart("[%s:%-3d] " fmt, lm::logging::_rawfilename(std::source_location::current().file_name()), std::source_location::current().line(), ##__VA_ARGS__)
    #define LOOM_RTRACE           lm::logging::logf_uart
    #define LOOM_BTRACE           lm::logging::lograw_uart

    // Inits the general CDC/HID logging task. NOTE: requires bus to be initialized otherwise it will crash.
    auto init() -> void;
    auto logf(const char* fmt, ...) -> int;
    auto lograw(const char* bytes, u32 len) -> void;
}

namespace lm{ namespace log = logging; }
