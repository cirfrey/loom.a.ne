#include "lm/log.hpp"

#include "lm/chip/system.hpp"
#include "lm/chip/time.hpp"
#include "lm/chip/uart.hpp"
#include "lm/board.hpp"

#include "lm/fabric/strand.hpp"
#include "lm/core/math.hpp"

#include "lm/strands/log.hpp"

#include <atomic>
#include <cstdio>
#include <cstdarg>

static constexpr auto get_short_filename(lm::log::fmt_t f) -> char const*
{
    auto start = f.loc.file_name();
    auto end = start;
    while(*++end != '\0'); // Find the end of the string.

    // Move back until we hit a slash OR the start of the string
    while(end > start && *(end - 1) != '\\' && *(end - 1) != '/') {
        --end;
    }
    return end;
}

auto lm::log::init() -> void
{
    lm::strands::log::init();
}

auto lm::log::fmt(mut_text in, fmt_t f, ...) -> mut_text
{
    auto out = mut_text{.data = in.data, .size = 0};

    if(f.args.color == feature::on)
    {
        auto ansi = config.logging.level_ansi[f.args.loglevel];
        out.size += std::snprintf(
            out.data + out.size, in.size - out.size, "%.*s", (int)ansi.size, ansi.data
        );
        out.size = clamp(out.size, 0, in.size);
    }

    if(f.args.prefix == feature::on)
    {
        auto prefix = config.logging.level_prefix[f.args.loglevel];
        out.size += std::snprintf(
            out.data + out.size, in.size - out.size, "%.*s", (int)prefix.size, prefix.data
        );
        out.size = clamp(out.size, 0, in.size);
    }

    if(f.args.timestamp == timestamp_t::timestamp_ms_6) {
        out.size += std::snprintf(
            out.data + out.size, in.size - out.size, "[%6lu]",
            chip::time::uptime()/1000
        );
        out.size = clamp(out.size, 0, in.size);
    }

    if(f.args.filename != filename_t::no_filename) {
        out.size += std::snprintf(
            out.data + out.size, in.size - out.size, "[%s:%-3d] ",
            f.args.filename == filename_t::short_filename ? get_short_filename(f) : f.loc.file_name(),
            f.loc.line()
        );
        out.size = clamp(out.size, 0, in.size);
    }

    va_list args;
    va_start(args, f);
    out.size += std::vsnprintf(out.data + out.size, in.size - out.size, f.args.fmt, args);
    out.size = clamp(out.size, 0, in.size);
    va_end(args);

    return out;
}

// TODO: configurable dispatch streams per loglevel?
auto lm::log::dispatch(text t, level loglevel) -> bool
{
    if(config.logging.custom_dispatcher != nullptr)
        return config.logging.custom_dispatcher(t, loglevel);
    if(config.logging.toggle == feature::off)
        return false;
    if(loglevel == level::panic)
        chip::system::panic(t, 1);
    return lm::strands::log::dispatch(t);
}

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
        if(yield) fabric::strand::sleep_ms(0); // Just yield.
    }

    auto tot = 0;
    while(tot < b.size) {
        tot += chip::uart::write(p, {(u8*)b.data + tot, b.size - tot});
    }

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
        if(yield) fabric::strand::sleep_ms(0); // Just yield.
    }

    auto tot = 0;
    while(tot < b.size) {
        tot += chip::uart::write(p, {(u8*)b.data + tot, b.size - tot});
    }

    uart_busy[p].clear(std::memory_order_release);
    return true;
}
