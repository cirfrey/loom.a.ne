#include "lm/log.hpp"

#include "lm/chip/time.hpp"
#include "lm/chip/uart.hpp"
#include "lm/board.hpp"

#include "lm/fabric/task.hpp"
#include "lm/core/math.hpp"

#include "lm/tasks/log.hpp"

#include <atomic>
#include <cstdio>
#include <cstdarg>

auto lm::log::fmt(mut_text in, fmt_t f, ...) -> mut_text
{
    auto out = mut_text{.data = in.data, .size = 0};

    char const* color = color_of[f.args.severity];
    out.size += std::snprintf(
        out.data + out.size, in.size - out.size, "%s", color
    );
    out.size = clamp(out.size, 0, in.size);

    if(f.args.timestamp == timestamp_ms_6) {
        out.size += std::snprintf(
            out.data + out.size, in.size - out.size, "[%6llu]",
            chip::time::uptime()/1000
        );
        out.size = clamp(out.size, 0, in.size);
    }

    if(f.args.filename != no_filename) {
        out.size += std::snprintf(
            out.data + out.size, in.size - out.size, "[%s:%-3d] ",
            f.args.filename == short_filename ? f.get_short_filename() : f.loc.file_name(),
            f.loc.line()
        );
        out.size = clamp(out.size, 0, in.size);
    }

    va_list args;
    va_start(args, f);
    out.size += std::vsnprintf(out.data + out.size, in.size - out.size, f.fmt, args);
    out.size = clamp(out.size, 0, in.size);
    va_end(args);

    return out;
}

auto lm::log::dispatch(text t) -> bool
{ return lm::tasks::log::dispatch(t); }

static std::atomic_flag uart_busy[lm::board::uart_port_count] = ATOMIC_FLAG_INIT;
auto lm::log::dispatch_immediate(chip::uart_port p, buf b, st timeout_micros, bool yield) -> void
{
    const auto start = chip::time::uptime();
    bool forced = false;

    while (uart_busy[p].test_and_set(std::memory_order_acquire))
    {
        if ((chip::time::uptime() - start) > timeout_micros) {
            forced = true;
            break; // Emergency: We've waited too long, proceed anyway
        }
        if(yield) fabric::task::sleep_ms(0); // Just yield.
    }

    chip::uart::write(p, b);

    if (!forced) {uart_busy[p].clear(std::memory_order_release);}
}

auto lm::log::try_dispatch_immediate(chip::uart_port p, buf b, st timeout_micros, bool yield) -> bool
{
    const auto start = chip::time::uptime();

    while (uart_busy[p].test_and_set(std::memory_order_acquire))
    {
        if ((chip::time::uptime() - start) > timeout_micros) {
            return false;
        }
        if(yield) fabric::task::sleep_ms(0); // Just yield.
    }

    chip::uart::write(p, b);

    uart_busy[p].clear(std::memory_order_release);
    return true;
}
