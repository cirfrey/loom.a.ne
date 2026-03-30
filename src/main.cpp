#include "lm/board.hpp"

#include "lm/chip/uart.hpp"
#include "lm/chip/system.hpp"
#include "lm/chip/time.hpp"

#include "lm/version/defs.hpp"
#include "lm/version/banner.hpp"
#include "lm/task.hpp"
#include "lm/tasks/sysman.hpp"

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
    chip::uart::init(board::uart_trace, board::gpio17, board::gpio18);

    // By printing the banner we make sure that things like the mac address are
    // initialized and can be used by the rest of the code.
    char bootstrbuf[12];
    auto bootstr = text{
        .data = bootstrbuf,
        .size = (st)std::snprintf(bootstrbuf, 12, "\n[%2llu] Boot\n", chip::time::uptime()/1000)
    };
    version::write_banner(
        board::uart_trace,
        version::major, version::minor,
        version::git_hash, version::build_date,
        bootstr
    );
    task::sleep_ms(500); // Take a moment to appreciate the banner, it's pretty.

    sysman::init();
}
