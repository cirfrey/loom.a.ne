#include "lm/entrypoint.hpp"

#include "lm/core.hpp"
#include "lm/chip.hpp"
#include "lm/board.hpp"
#include "lm/config.hpp"
#include "lm/build.hpp"
#include "lm/log.hpp"
#include "lm/banner.hpp"

#include "lm/fabric/strand.hpp"

#include "lm/strands/strandman.hpp"
#include "lm/strands/log.hpp"
#include "lm/strands/healthmon.hpp"
#include "lm/strands/blink.hpp"
#include "lm/strands/busmon.hpp"
#include "lm/strands/usbd.hpp"
#include "lm/strands/usbip.hpp"
#include "lm/strands/apply_config.hpp"

#include <cstdio>

#include "lm/ini.hpp"

extern "C" auto loomane_default_entrypoint() -> void
{
    using namespace lm;

    chip::system::init();
    chip::uart::init(board::uart_trace, board::gpio_uart_trace_tx, board::gpio_uart_trace_rx);

    // By printing the banner we make sure that things like the mac address are
    // initialized and can be used by the rest of the code.
    char bootstrbuf[64];
    auto bootstr = log::fmt(
        {bootstrbuf, sizeof(bootstrbuf)},
        log::fmt_t({ .fmt = "%s\n", .loglevel = log::level::debug }),
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

    using info = strands::strandman::strand_info_t;
    auto logging_ready = info::depends_t {
        .my_status    = info::requested_creation,
        .other_id     = 1,
        .other_status = info::ready,
    };
    auto config_applied = info::depends_t{
        .my_status    = info::requested_creation,
        .other_id     = 2,
        .other_status = info::stopped
    };
    info default_strands[] = {
        // These always start up.
        info{
            .code      = [](void*){}, // Dummy.
            .constants = _config::strand::strandman,
            .runtime   = { .id = 0, .sleep_ms = 10 },
        },
        info{
            .code      = fabric::strand::managed<strands::log>(),
            .constants = _config::strand::logging,
            .runtime   = { .id = logging_ready.other_id, .sleep_ms = 10 },
            .should_be_running = true,
        },

        // Then we apply config, this depends on the logbuf being created, otherwise
        // it'll discard the warnings.
        info{
            .code      = fabric::strand::managed<strands::apply_config>(),
            .constants = _config::strand::apply_config,
            .runtime   = { .id = config_applied.other_id, .sleep_ms = 1 },
            .depends   = {logging_ready},
            .should_be_running = true,
        },

        // Then these depends on config_applied,
        info{
            .code      = fabric::strand::managed<strands::healthmon>(),
            .constants = _config::strand::healthmon,
            .runtime   = { .id = 3, .sleep_ms = 3000 },
            .depends   = {config_applied},
            .should_be_running = true,
        },
        info{
            .code      = fabric::strand::managed<strands::blink>(),
            .constants = _config::strand::blink,
            .runtime   = { .id = 4, .sleep_ms = 10 },
            .depends   = {config_applied},
            .should_be_running = true
        },
        info{
            .code      = fabric::strand::managed<strands::busmon>(),
            .constants = _config::strand::busmon,
            .runtime   = { .id = 5, .sleep_ms = 10 },
            .depends   = {config_applied},
            .should_be_running = false,
        },
        info{
            .code      = fabric::strand::managed<strands::usbd>(),
            .constants = _config::strand::usbd,
            .runtime   = { .id = 6, .sleep_ms = 1000 },
            .depends   = {config_applied},
            .should_be_running = true,
        },
        info{
            .code      = fabric::strand::managed<strands::usbip>(),
            .constants = _config::strand::usbip,
            .runtime   = { .id = 7, .sleep_ms = 1000 },
            .depends   = {config_applied},
            .should_be_running = true,
        },
    };

    // TODO: configurable max strands.
    strands::strandman::spawn<10>(
        default_strands[0],
        {&default_strands[1], (sizeof(default_strands)/sizeof(info)) - 1}
    );
}
