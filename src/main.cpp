#include "lm/board.hpp"

#include "lm/chip/uart.hpp"
#include "lm/chip/system.hpp"
#include "lm/chip/time.hpp"

#include "lm/build.hpp"
#include "lm/version/banner.hpp"
#include "lm/fabric/task.hpp"
#include "lm/log.hpp"

#include "lm/tasks/tman.hpp"
#include "lm/tasks/log.hpp"
#include "lm/tasks/healthmon.hpp"
#include "lm/tasks/blink.hpp"
#include "lm/tasks/busmon.hpp"
#include "lm/tasks/usbd.hpp"
#include "lm/config.hpp"

#include <cstdio>

// app_main is only the entrypoint/launcher to the application, their responsibilities are
// - Init the hardware.
// - Let sysman init the software and handle all the task orchestration.
// It should not:
// - Run application code, that would be the lm::app::task.
//
// NOTE: app_main is special and doesn't need to be terminated (vTaskDelete),
//       whoever spawned it will take care of the cleanup.
extern "C" auto app_main() -> void
{
    using namespace lm;

    chip::system::init();
    chip::uart::init(board::uart_trace, board::gpio_uart_trace_tx, board::gpio_uart_trace_rx);

    // By printing the banner we make sure that things like the mac address are
    // initialized and can be used by the rest of the code.
    char bootstrbuf[17];
    auto bootstr = text{
        .data = bootstrbuf,
        .size = (st)std::snprintf(bootstrbuf, 17, "\n%s[%2llu] Boot\n", log::color_of[log::severity_debug], chip::time::uptime()/1000)
    };
    version::write_banner(
        [](text t){ log::dispatch_immediate(board::uart_trace, t, 0, true); },
        build::version_major, build::version_minor,
        build::git_hash, build::build_date,
        bootstr
    );
    fabric::task::sleep_ms(500); // Take a moment to appreciate the banner, it's pretty.

    tasks::tman::spawn<10>({
        .code      = [](void*){}, // Dummy.
        .constants = config::task::tman,
        .runtime   = {
            .id = 0,
            .sleep_ms = 10,
        }},
        {{
            .code      = fabric::task::managed<tasks::log>(),
            .constants = config::task::logging,
            .runtime   = { .id = 1, .sleep_ms = 10 },
            .should_be_running = true,
        }, {
            .code      = fabric::task::managed<tasks::healthmon>(),
            .constants = config::task::healthmon,
            .runtime   = { .id = 2, .sleep_ms = 3000 },
            .should_be_running = true,
        }, {
            .code      = fabric::task::managed<tasks::blink>(),
            .constants = config::task::blink,
            .runtime   = { .id = 3, .sleep_ms = 10 },
            .should_be_running = true
        }, {
            .code      = fabric::task::managed<tasks::busmon>(),
            .constants = config::task::busmon,
            .runtime   = { .id = 4, .sleep_ms = 10 },
            .should_be_running = false,
        }, {
            .code      = fabric::task::managed<tasks::usbd>(),
            .constants = config::task::usbd,
            .runtime   = { .id = 5, .sleep_ms = 1000 },
            .should_be_running = true,
        }}
    );
}
