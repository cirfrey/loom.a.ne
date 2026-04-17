#include "lm/hooks.hpp"

#include "lm/core.hpp"
#include "lm/board.hpp"
#include "lm/config.hpp"
#include "lm/log.hpp"

#include "lm/fabric/strand.hpp"
#include "lm/strands/strandman.hpp"
#include "lm/strands/log.hpp"
#include "lm/strands/healthmon.hpp"
#include "lm/strands/blink.hpp"
#include "lm/strands/busmon.hpp"
#include "lm/strands/usbd.hpp"
#include "lm/strands/usbip.hpp"
#include "lm/strands/apply_config.hpp"


auto lm::hook::launcher() -> void
{
    if(lm::hook::launcher_override != nullptr) {
        lm::hook::launcher_override();
        return;
    }

    lm::hook::arch_init();
    if(lm::hook::init != nullptr)
        lm::hook::init();

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
            .should_be_running = lm::config.usb.strand.spawn == feature::on,
        },
        info{
            .code      = fabric::strand::managed<strands::usbip>(),
            .constants = _config::strand::usbip,
            .runtime   = { .id = 7, .sleep_ms = 1 },
            .depends   = {config_applied},
            .should_be_running = lm::config.usbip.strand.spawn == feature::on,
        },
    };

    // TODO: configurable max strands.
    strands::strandman::spawn<10>(
        default_strands[0],
        {&default_strands[1], (sizeof(default_strands)/sizeof(info)) - 1}
    );
}
