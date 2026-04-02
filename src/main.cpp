#include "lm/board.hpp"

#include "lm/chip/uart.hpp"
#include "lm/chip/system.hpp"
#include "lm/chip/time.hpp"

#include "lm/version/defs.hpp"
#include "lm/version/banner.hpp"
#include "lm/fabric/task.hpp"

#include "lm/log.hpp"
//#include "lm/tasks/sysman.hpp"

#include "lm/tasks/log.hpp"
#include "lm/fabric.hpp"


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
    char bootstrbuf[16];
    auto bootstr = text{
        .data = bootstrbuf,
        .size = (st)std::snprintf(bootstrbuf, 16, "\n%s[%2llu] Boot\n", log::color_of[log::severity_debug], chip::time::uptime()/1000)
    };
    version::write_banner(
        [](text t){ log::dispatch_immediate(board::uart_trace, t, 0, true); },
        version::major, version::minor,
        version::git_hash, version::build_date,
        bootstr
    );
    fabric::task::sleep_ms(500); // Take a moment to appreciate the banner, it's pretty.

    auto status_q = fabric::queue<fabric::event>(32);
    auto status_q_tok = fabric::bus::subscribe(status_q, fabric::topic::task, {
        fabric::task::event::created,
        fabric::task::event::ready,
        fabric::task::event::running,
        fabric::task::event::waiting_for_reap,
    });

    fabric::task::create(fabric::task_constants{
        .name = "task::logger",
        .priority = 2,
        .stack_size = 4096,
        .core_affinity = 0,
    }, fabric::task_runtime_info{
        .id = 1,
        .sleep_ms = 10,
    }, fabric::task::managed<tasks::log>());

    st loopid = 0;
    while(1){
        for(auto const& e : status_q.consume<fabric::event>()) {
            switch(e.type) {
                case fabric::task::event::ready:
                    fabric::bus::publish(fabric::event{
                        .topic     = fabric::topic::task,
                        .type      = fabric::task::event::signal_start,
                        .sender_id = 0,
                    }.with_payload(fabric::task::signal_event{ .task_id = 1 }));
                default: break;
            }
        }
        fabric::task::sleep_ms(10);

        if(loopid % 4 == 0)
            log::debug("Loopid = %zu\n", loopid++);
        else if(loopid % 4 == 1)
            log::info("Loopid = %zu\n", loopid++);
        else if(loopid % 4 == 2)
            log::warn("Loopid = %zu\n", loopid++);
        else if(loopid % 4 == 3)
            log::error("Loopid = %zu\n", loopid++);
    }
    //sysman::init();
}
