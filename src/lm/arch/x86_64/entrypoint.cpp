#include "lm/entrypoint.hpp"

#include "lm/board.hpp"
#include "lm/chip.hpp"
#include "lm/log.hpp"
#include "lm/fabric.hpp"
#include "lm/build.hpp"
#include "lm/banner.hpp"

auto lm::entrypoint::arch_init() -> void
{
    chip::system::init();
    chip::uart::init(board::uart_trace, board::gpio_uart_trace_tx, board::gpio_uart_trace_rx);

    // By printing the banner we make sure that things like the mac address are
    // initialized and can be used by the rest of the code.
    char bootstrbuf[64];
    auto bootstr = log::fmt(
        {bootstrbuf, sizeof(bootstrbuf)},
        log::fmt_t({ .fmt = "%s\n", .loglevel = log::level::info }),
        "Welcome to Loomane :)"
    );
    write_banner(
        [](text t){ log::dispatch_immediate(board::uart_trace, t, 0, true); },
        [](){ fabric::strand::sleep_ms(1); },
        {bootstr.data, bootstr.size},
        {
            .ver_major = build::version_major,
            .ver_minor = build::version_minor,
            .banner = chip::info::banner(),
            .uuid = chip::info::uuid(),
            .chip_name = chip::info::name(),
            .total_ram = chip::memory::total(),
            .free_ram = chip::memory::free(),
            .uptime_us = chip::time::uptime(),
            .temp = chip::sensor::internal_temperature(),
            .arch = build::arch,
            .board = build::board,
            .git_hash = build::git_hash,
            .build_date = build::build_date,
        }
    );
    fabric::strand::sleep_ms(500); // Take a moment to appreciate the banner, it's pretty.
}



#include "lm/arch/x86_64/endpoints.hpp"

auto lm::entrypoint::arch_config() -> void
{
    config.usb.endpoints = arch::x86_64::endpoints;
}
