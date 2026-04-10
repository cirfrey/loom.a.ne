#include "lm/board.hpp"

#include "lm/chip.hpp"

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

#include "lm/core.hpp"
#include "lm/usbip.hpp"

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
        .size = (st)std::snprintf(bootstrbuf, 17, "\n%s[%2lu] Boot\n", log::color_of[log::severity_debug], chip::time::uptime()/1000)
    };
    version::write_banner(
        [](text t){ log::dispatch_immediate(board::uart_trace, t, 0, true); },
        [](){ fabric::task::sleep_ms(1); },
        bootstr,
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


#ifdef LOOMANE_NATIVE

#if defined(_WIN32) || defined(__CYGWIN__)
    // Native Windows (MSVC, MinGW, Clang-cl)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <atomic>

    std::atomic<bool> quit = false;
    BOOL WINAPI console_ctrl_handler(DWORD dwCtrlType) {
        if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT) {
            quit = true;
            // Returning TRUE tells Windows we handled it
            return TRUE;
        }
        return FALSE;
    }
    auto main() -> int
    {
        SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
        app_main();
        while(!quit.load()) lm::fabric::task::sleep_ms(1000);
        return 0;
    }

#else

    auto main() -> int
    {
        app_main();
        while(1) lm::fabric::task::sleep_ms(10000);
        return 0;
    }

#endif

#endif
