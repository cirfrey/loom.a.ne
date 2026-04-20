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

// TODO: allow override.
auto lm::hook::launcher() -> void
{
    lm::hook::framework_init();
    lm::hook::arch_init();
    lm::hook::init();

    lm::hook::arch_config();
    lm::hook::config();

    if(lm::config.test.unit == feature::on)
        lm::hook::test::unit();

    lm::hook::framework_main();
    lm::hook::main();
}

auto lm::hook::framework_init() -> void
{
    // We also allocate id(0) since it has special meaning (any strand) and we don't want to have
    // a strand running with that id.
    lm::fabric::strand::registry::init();
    if(lm::fabric::strand::registry::reserve(0).has_error()) {
        log::panic("lm::fabric::strand::registry::reserve(0) failed, somethine is seriously wrong!\n");
        chip::system::panic({}, 1);
    }

    // Doesn't start the log task, just inits the buffer so we don't lose any messages until it does
    // actually spin up.
    lm::log::init();
}

auto lm::hook::framework_main() -> void
{
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
        chip::system::halt(1);
    }

    auto ret = fabric::register_strand({
        .name       = "lm.log"_text,
        .stack_size = lm::config.launcher.log.stack_size,
        .sleep_ms   = 10,
        .code       = fabric::strand::managed<strands::log>(),
    });
    if(!ret.ok()) {
        lm::log::panic("Failed to spawn log strand! Something very bad happened.\n");
        chip::system::halt(1);
    }

    // TODO: subscribe to strandman and wait for it to boot, then send request
    //       to start all the strands we need from lm::config.
}

[[gnu::weak]] auto lm::hook::init() -> void {}
[[gnu::weak]] auto lm::hook::config() -> void {}
[[gnu::weak]] auto lm::hook::test::unit() -> void {}
[[gnu::weak]] auto lm::hook::main() -> void {}
