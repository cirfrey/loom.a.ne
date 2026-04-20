#include "lm/hooks.hpp"

#include "lm/core.hpp"
#include "lm/board.hpp"
#include "lm/config.hpp"
#include "lm/log.hpp"
#include "lm/chip/system.hpp"

#include "lm/fabric/strand.hpp"
#include "lm/fabric/strand_registry.hpp"
#include "lm/fabric/register_strand.hpp"

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

    // We also allocate id(0) since it has special meaning (any strand) and we don't want to have
    // a strand running with that id.
    lm::fabric::strand::registry::init();
    if(lm::fabric::strand::registry::reserve(0).has_error()) {
        lm::log::panic("lm::fabric::strand::registry::reserve(0) failed, somethine is seriously wrong!\n");
    }

    lm::hook::arch_init();
    if(lm::hook::init != nullptr)
        lm::hook::init();

    // Doesn't start the log task, just inits the buffer so we don't lose any messages until it does
    // actually spin up.
    lm::log::init();

    if(lm::hook::test::unit != nullptr && lm::config.launcher.test.unit == feature::on)
        lm::hook::test::unit();

    // using info = strands::strandman::strand_info_t;
    // auto logging_ready = info::depends_t {
    //     .my_status    = info::requested_creation,
    //     .other_id     = 1,
    //     .other_status = info::ready,
    // };
    // auto config_applied = info::depends_t{
    //     .my_status    = info::requested_creation,
    //     .other_id     = 2,
    //     .other_status = info::stopped
    // };
    // info default_strands[] = {
    //     // These always start up.
    //     info{
    //         .code      = [](void*){}, // Dummy.
    //         .constants = _config::strand::strandman,
    //         .runtime   = { .id = 0, .sleep_ms = 10 },
    //     },
    //     info{
    //
    //     },

    //     // Then we apply config, this depends on the logbuf being created, otherwise
    //     // it'll discard the warnings.
    //     info{
    //         .code      = fabric::strand::managed<strands::apply_config>(),
    //         .constants = _config::strand::apply_config,
    //         .runtime   = { .id = config_applied.other_id, .sleep_ms = 1 },
    //         .depends   = {logging_ready},
    //     },

    //     // Then these depends on config_applied,
    //     info{
    //         .code      = fabric::strand::managed<strands::healthmon>(),
    //         .constants = _config::strand::healthmon,
    //         .runtime   = { .id = 3, .sleep_ms = 3000 },
    //         .depends   = {config_applied},
    //     },
    //     info{
    //         .code      = fabric::strand::managed<strands::blink>(),
    //         .constants = _config::strand::blink,
    //         .runtime   = { .id = 4, .sleep_ms = 10 },
    //         .depends   = {config_applied},
    //     },
    //     info{
    //         .code      = fabric::strand::managed<strands::busmon>(),
    //         .constants = _config::strand::busmon,
    //         .runtime   = { .id = 5, .sleep_ms = 10 },
    //         .depends   = {config_applied},
    //     },
    //     // TODO: somehow we need to defer these from running until configuration is parsed, otherwise
    //     //       lm::config.usb.strand.spawn will always be set to the default (will not respect overrides).
    //     info{
    //         .code      = fabric::strand::managed<strands::usbd>(),
    //         .constants = _config::strand::usbd,
    //         .runtime   = { .id = 6, .sleep_ms = 1000 },
    //         .depends   = {config_applied},
    //     },
    //     info{
    //         .code      = fabric::strand::managed<strands::usbip>(),
    //         .constants = _config::strand::usbip,
    //         .runtime   = { .id = 7, .sleep_ms = 1 },
    //         .depends   = {config_applied},
    //     },
    // };

    auto strandman_handle = strands::strandman::spawn< config_t::launcher_t::strandman_max_strands >(
        strands::strandman::strand_t{
            .code = [](void*){}, // Dummy.
            .id = lm::fabric::strand::registry::reserve().id,
            .stack_size = lm::config.launcher.strandman.stack_size,
            .sleep_ms = 0,
            .name = "lm.strandman",
        }
    );
    if(!strandman_handle) {
        lm::log::panic("Failed to spawn strandman! Nothing can be done.\n");
    }

    auto ret = fabric::register_strand({
        .name       = "lm.log"_text,
        .stack_size = lm::config.launcher.log.stack_size,
        .sleep_ms   = 10,
        .code       = fabric::strand::managed<strands::log>(),
    });
    if(!ret.ok()) { lm::log::panic("Failed to spawn log strand! Something very bad happened.\n"); }


    // TODO: subscribe to strandman and wait for it to boot, then send request
    //       to start all the strands we need from lm::config.

    if(lm::hook::main != nullptr)
        lm::hook::main();
}
