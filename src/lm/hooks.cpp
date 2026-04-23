#include "lm/hooks.hpp"

#include "lm/core.hpp"
#include "lm/board.hpp"
#include "lm/config.hpp"
#include "lm/registry.hpp"
#include "lm/config/boot.hpp"
#include "lm/log.hpp"
#include "lm/chip/system.hpp"
#include "lm/config/ini.hpp"

#include "lm/fabric/strand.hpp"
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
    lm::hook::framework_init(config_rw);
    lm::hook::arch_init(config_rw);
    lm::hook::init(config_rw);

    lm::hook::framework_config(config_rw);
    lm::hook::arch_config(config_rw);
    lm::hook::config(config_rw);

    lm::hook::parse_ini(config_rw);

    if(lm::config.test.unit == feature::on)
        lm::hook::test::unit(config_rw);

    lm::hook::framework_main();
    lm::hook::main();
}

auto lm::hook::framework_init(config_t& config) -> void
{
    registry::strand_id.init();

    // Doesn't start the log task, just inits the buffer so we don't lose any messages until it does
    // actually spin up.
    log::init();
}

auto lm::hook::framework_config(config_t& config) -> void
{
    config.launcher.log.code       = fabric::strand::managed<strands::log>();
    config.launcher.healthmon.code = fabric::strand::managed<strands::healthmon>();
    config.launcher.blink.code     = fabric::strand::managed<strands::blink>();
    config.launcher.busmon.code    = fabric::strand::managed<strands::busmon>();
    config.launcher.usbd.code      = fabric::strand::managed<strands::usbd>();
    config.launcher.usbip.code     = fabric::strand::managed<strands::usbip>();
}

auto lm::hook::parse_ini(config_t& config) -> void
{
    if(config.ini.with_source == nullptr) {
        log::warn("lm::config.ini.with_source not set, this is likely an oversight in lm::hooks::arch_config\n");
        return;
    }

    config.ini.with_source(nullptr, [](void*, text ini_text){
        auto result = ini::parse(ini_text, config_ini::fields);

        if(result != lm::ini::parse_result::ok) {
            auto re = renum<lm::ini::parse_result>::unqualified(result);
            lm::log::warn("Ini parsed with errors, result=[%.*s]\n", (int)re.size, re.data);
        }
    });
}

auto lm::hook::framework_main() -> void
{
    auto strandman_arg = strands::strandman::strand_t{
        .code          = [](void*){}, // Dummy.
        .id            = lm::registry::strand_id.reserve().id | toe,
        .stack_size    = lm::config.launcher.strandman.stack_size,
        .sleep_ms      = lm::config.launcher.strandman.sleep_ms,
        .priority      = lm::config.launcher.strandman.priority,
        .core_affinity = lm::config.launcher.strandman.core_affinity,
    };
    std::memcpy(strandman_arg.name, lm::config.launcher.strandman.name, sizeof(lm::config.launcher.strandman.name));
    auto strandman_handle = strands::strandman::spawn< config_t::launcher_t::strandman_max_strands >(
        strandman_arg
    );
    if(!strandman_handle) {
        lm::log::panic("Failed to spawn strandman! Nothing can be done.\n");
        chip::system::halt(1);
    }

    for(auto& info : {
        lm::config.launcher.log,
        lm::config.launcher.healthmon,
        lm::config.launcher.blink,
        lm::config.launcher.busmon,
        lm::config.launcher.usbd,
        lm::config.launcher.usbip,
    })
    {
        if(!fabric::register_strand({
            .name          = info.name | rc<char const*> | to_text,
            .stack_size    = info.stack_size,
            .sleep_ms      = info.sleep_ms,
            .priority      = info.priority,
            .core_affinity = info.core_affinity,
            .start         = info.set_running == feature::on,
            .code          = info.code,
        }).ok())
        {
            lm::log::panic("Failed to register strand [%s]! Something very bad happened.\n", info.name);
            chip::system::halt(1);
        }
    }

    // TODO: subscribe to strandman and wait for it to boot, then send request
    //       to start all the strands we need from lm::config.
}

[[gnu::weak]] auto lm::hook::init(config_t&) -> void {}
[[gnu::weak]] auto lm::hook::config(config_t&) -> void {}
[[gnu::weak]] auto lm::hook::test::unit(config_t&) -> void {}
[[gnu::weak]] auto lm::hook::main() -> void {}
