#pragma once

#include "lm/chip/types.hpp"
#include "lm/core/types.hpp"
#include "lm/config.hpp"
#include "lm/board.hpp"

#include "lm/core/veil.hpp"

#include <source_location>

namespace lm::log
{
    using timestamp_t = config_t::logging_t::timestamp_t;
    using filename_t = config_t::logging_t::filename_t;
    using color_t = feature;
    using prefix_t = feature;
    using level = config_t::logging_t::level_t;

    struct fmt_t_args {
        const char* fmt = "";
        timestamp_t timestamp = timestamp_t::ms_6;
        filename_t  filename  = filename_t::file;
        color_t     color     = feature::on;
        prefix_t    prefix    = feature::on;
        level       loglevel  = level::debug;

        static auto from_config() -> fmt_t_args
        {
            return {
                .timestamp = config.logging.timestamp,
                .filename  = config.logging.filename,
                .color     = config.logging.color,
                .prefix    = config.logging.prefix,
            };
        }

        static auto clean() -> fmt_t_args
        {
            return {
                .timestamp = timestamp_t::no_timestamp,
                .filename  = filename_t::no_filename,
                .color     = feature::off,
                .prefix    = feature::off,
            };
        }

        constexpr auto with_fmt(const char* f) -> fmt_t_args&
        { fmt = f; return *this; }
        constexpr auto with_timestamp(timestamp_t t) -> fmt_t_args&
        { timestamp = t; return *this; }
        constexpr auto with_filename(filename_t f) -> fmt_t_args&
        { filename = f; return *this; }
        constexpr auto with_color(color_t c) -> fmt_t_args&
        { color = c; return *this; }
        constexpr auto with_prefix(prefix_t p) -> fmt_t_args&
        { prefix = p; return *this; }
        constexpr auto with_loglevel(level l) -> fmt_t_args&
        { loglevel = l; return *this; }
    };

    struct fmt_t
    {
        fmt_t_args args;
        std::source_location loc;

        constexpr fmt_t(
            const char* f,
            fmt_t_args a = fmt_t_args::from_config(),
            std::source_location l = std::source_location::current()
        ) : args(a), loc(l) { args.fmt = f; }

        constexpr fmt_t(
            fmt_t_args a,
            std::source_location l = std::source_location::current()
        ) : args(a), loc(l) {}
    };

    // Forces initialization of the internal structures for logging.
    // Useful if you dont want to miss a log before the logging strand is up.
    // It'll only be printed once its up but atleast you won't miss it.
    auto init() -> void;

    [[nodiscard]] auto fmt(mut_text in, fmt_t f, ...) -> mut_text;

    // The underlying log function used by the shorthands. Just formats and filters by loglevel, defers the actual logging to dispatch().
    template<u16 Bufsize = config_t::logging_t::format_bufsize, typename... Args> constexpr auto log(fmt_t f, Args&&...) -> bool;

    // Use this if you define your own level shorthands. Don't forget to namespace it appropriately.
    #define LM_LOG_DECLARE_LEVEL_SHORTHAND(LEVEL)                                                                    \
        template<lm::u16 BufSize = config_t::logging_t::format_bufsize, typename... Args>                            \
        constexpr auto LEVEL(fmt_t f, Args&&... args) -> bool                                                        \
        { f.args.loglevel = level::LEVEL; return lm::log::log<BufSize>(f, veil::forward<decltype(args)>(args)...); }

    // The shorthands for the default loom.a.ne levels.
    // With them you can do lm::log::debug("my fmt str %s\n", "and args");
    // Intead of calling lm::log::log directly with annoying syntax

    LM_LOG_DECLARE_LEVEL_SHORTHAND(debug)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(test)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(assertion)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(info)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(regular)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(warn)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(error)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(panic)

    // Pushes the log to the ringbuffer so the logger strand can take care of it.
    auto dispatch(text t, level loglevel) -> bool;
    // Just pushes it to UART with complete disregard for ethics and morals.
    auto dispatch_immediate(chip::uart_port, buf, st timeout_micros, bool yield) -> void;
    template <u16 BufSize = config_t::logging_t::format_bufsize, typename... Args>
    constexpr auto dispatch_immediate(chip::uart_port, st, bool, fmt_t, Args... args) -> void;
    // Tries to push to UART with complete regard for ethics and morals (gives up if busy).
    auto try_dispatch_immediate(chip::uart_port, buf, st timeout_micros, bool yield) -> bool;
    template <u16 BufSize = config_t::logging_t::format_bufsize, typename... Args>
    constexpr auto try_dispatch_immediate(chip::uart_port, st, bool, fmt_t, Args... args) -> bool;
}


/* --- log impls --- */

template<lm::u16 BufSize, typename... Args>
constexpr auto lm::log::log(fmt_t f, Args&&... args) -> bool
{
    if(config.logging.level[f.args.loglevel].enabled == feature::off) return false;

    char buf[BufSize];
    auto formatted = fmt({buf, sizeof(buf)}, f, veil::forward<Args>(args)...);
    return dispatch(formatted, f.args.loglevel);
}

template <lm::u16 BufSize, typename... Args>
constexpr auto lm::log::dispatch_immediate(chip::uart_port port, st timeout_micros, bool yield, fmt_t f, Args... args) -> void
{
    char buf[BufSize];
    auto formatted = fmt({buf, sizeof(buf)}, f, veil::forward<Args>(args)...);
    dispatch_immediate(port, formatted, timeout_micros, yield);
}

template <lm::u16 BufSize, typename... Args>
constexpr auto lm::log::try_dispatch_immediate(chip::uart_port port, st timeout_micros, bool yield, fmt_t f, Args... args) -> void
{
    char buf[BufSize];
    auto formatted = fmt({buf, sizeof(buf)}, f, veil::forward<Args>(args)...);
    return try_dispatch_immediate(port, formatted, timeout_micros, yield);
}
