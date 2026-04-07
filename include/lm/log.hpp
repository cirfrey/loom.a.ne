#pragma once

#include "lm/chip/types.hpp"
#include "lm/core/types.hpp"
#include "lm/config.hpp"
#include "lm/board.hpp"

#include "lm/core/veil.hpp"

#include <source_location>

namespace lm::log
{
    struct fmt_t;
    enum timestamp_t {
        no_timestamp,
        timestamp_ms_6,
    };
    enum filename_t {
        no_filename,
        short_filename,
        full_filename,
    };
    enum severity_t {
        severity_debug,
        severity_info,
        severity_warn,
        severity_error,

        severity_none,
    };
    constexpr char const* color_of[] = {
        [severity_debug] = "\033[90m",
        [severity_info]  = "\033[37m",
        [severity_warn]  = "\033[33m",
        [severity_error] = "\033[31m",
        [severity_none]  = "\033[0m",
    };
    struct fmt_t_args {
        timestamp_t timestamp = timestamp_ms_6;
        filename_t  filename  = short_filename;
        severity_t  severity  = severity_debug;
    };

    [[nodiscard]] auto fmt(mut_text in, fmt_t f, ...) -> mut_text;

    // All of these use dispatch() and not dispatch_immediate().
    // If you want formatting for your dispatch_immediate() then you do it
    // yourself using fmt().
    template<typename... Args> constexpr auto log(severity_t, fmt_t f, Args&&...) -> bool;
    template<typename... Args> constexpr auto debug(fmt_t f, Args&&...) -> bool;
    template<typename... Args> constexpr auto info(fmt_t f, Args&&...) -> bool;
    template<typename... Args> constexpr auto warn(fmt_t f, Args&&...) -> bool;
    template<typename... Args> constexpr auto error(fmt_t f, Args&&...) -> bool;

    // Pushes the log to the ringbuffer so the logger task can take care of it.
    auto dispatch(text t) -> bool;
    // Just pushes it to UART with complete disregard for ethics and morals.
    auto dispatch_immediate(chip::uart_port, buf, st timeout_micros, bool yield) -> void;
    // Tries to push to UART with complete regard for ethics and morals (gives up if busy).
    auto try_dispatch_immediate(chip::uart_port, buf, st timeout_micros, bool yield) -> bool;
}




/* --- fmt_t impl --- */

struct lm::log::fmt_t
{
    constexpr auto get_short_filename() -> char const*
    {
        auto start = loc.file_name();
        auto end = start;
        while(*++end != '\0'); // Find the end of the string.

        // Move back until we hit a slash OR the start of the string
        while(end > start && *(end - 1) != '\\' && *(end - 1) != '/') {
            --end;
        }
        return end;
    }

    const char* fmt;
    fmt_t_args args;
    std::source_location loc;

    constexpr fmt_t(
        const char* f,
        fmt_t_args a = fmt_t_args{},
        std::source_location l = std::source_location::current()
    ) : fmt(f), args(a), loc(l) {}
};



/* --- log impls --- */

template<typename... Args>
constexpr auto lm::log::log(severity_t s, fmt_t f, Args&&... args) -> bool
{
    f.args.severity = s;
    char buf[lm::config::logging::log_format_bufsize];
    auto in  = mut_text{.data = buf, .size = sizeof(buf)};
    auto out = fmt(in, f, veil::forward<decltype(args)>(args)...);
    return dispatch(text{.data = out.data, .size = out.size});
}

template<typename... Args>
constexpr auto lm::log::debug(fmt_t f, Args&&... args) -> bool
{ return log(severity_debug, f, veil::forward<decltype(args)>(args)...); }

template<typename... Args>
constexpr auto lm::log::info(fmt_t f, Args&&... args) -> bool
{ return log(severity_info, f, veil::forward<decltype(args)>(args)...); }

template<typename... Args>
constexpr auto lm::log::warn(fmt_t f, Args&&... args) -> bool
{ return log(severity_warn, f, veil::forward<decltype(args)>(args)...); }

template<typename... Args>
constexpr auto lm::log::error(fmt_t f, Args&&... args) -> bool
{ return log(severity_error, f, veil::forward<decltype(args)>(args)...); }
