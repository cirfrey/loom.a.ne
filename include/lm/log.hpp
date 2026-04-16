#pragma once

#include "lm/chip/types.hpp"
#include "lm/core/types.hpp"
#include "lm/config.hpp"
#include "lm/board.hpp"

#include "lm/core/veil.hpp"

#include <source_location>

namespace lm::log
{
    enum timestamp_t {
        no_timestamp,
        timestamp_ms_6,
    };
    enum filename_t {
        no_filename,
        short_filename,
        full_filename,
    };
    using level = config_t::logging_t::level;

    struct fmt_t_args {
        const char* fmt = "";
        timestamp_t timestamp = timestamp_ms_6;
        filename_t  filename  = short_filename;
        level       loglevel  = level::debug;
    };

    struct fmt_t
    {
        fmt_t_args args;
        std::source_location loc;

        constexpr fmt_t(
            const char* f,
            fmt_t_args a = fmt_t_args{},
            std::source_location l = std::source_location::current()
        ) : args(a), loc(l) { args.fmt = f; }
        constexpr fmt_t(
            fmt_t_args a = fmt_t_args{},
            std::source_location l = std::source_location::current()
        ) : args(a), loc(l) {}
    };

    [[nodiscard]] auto fmt(mut_text in, fmt_t f, ...) -> mut_text;

    // All of these use dispatch() and not dispatch_immediate().
    // If you want formatting for your dispatch_immediate() then you do it yourself using fmt().
    template<u16 Bufsize = config_t::logging_t::format_bufsize, typename... Args> constexpr auto log(fmt_t f, Args&&...) -> bool;
    // Use this if you define your own level shorthands. Don't forget to namespace it appropriately.
    #define LM_LOG_DECLARE_LEVEL_SHORTHAND(LEVEL)                                                                    \
        template<lm::u16 BufSize = config_t::logging_t::format_bufsize, typename... Args>                            \
        constexpr auto LEVEL(fmt_t f, Args&&... args) -> bool                                                        \
        { f.args.loglevel = level::LEVEL; return lm::log::log<BufSize>(f, veil::forward<decltype(args)>(args)...); }
    // Then the shorthands for the default loom.a.ne levels.
    // Now you can do lm::log::debug("my fmt str %s\n", "and args");
    // Intead of calling lm::log::log directly with annoying syntax
    LM_LOG_DECLARE_LEVEL_SHORTHAND(debug)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(info)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(regular)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(warn)
    LM_LOG_DECLARE_LEVEL_SHORTHAND(error)

    // Pushes the log to the ringbuffer so the logger strand can take care of it.
    auto dispatch(text t) -> bool;
    // Just pushes it to UART with complete disregard for ethics and morals.
    auto dispatch_immediate(chip::uart_port, buf, st timeout_micros, bool yield) -> void;
    // Tries to push to UART with complete regard for ethics and morals (gives up if busy).
    auto try_dispatch_immediate(chip::uart_port, buf, st timeout_micros, bool yield) -> bool;
}


/* --- log impls --- */

template<lm::u16 BufSize, typename... Args>
constexpr auto lm::log::log(fmt_t f, Args&&... args) -> bool
{
    if(config.logging.level_enabled[f.args.loglevel] == config_t::feature::off) return false;

    char buf[BufSize];
    auto in  = mut_text{.data = buf, .size = sizeof(buf)};
    auto out = fmt(in, f, veil::forward<decltype(args)>(args)...);
    return dispatch(text{.data = out.data, .size = out.size});
}
