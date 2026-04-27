#include "lm/hooks.hpp"

#include "lm/core/all.hpp"
#include "lm/board.hpp"
#include "lm/registry.hpp"
#include "lm/log.hpp"
#include "lm/chip/all.hpp"
#include "lm/fabric/all.hpp"
#include "lm/build.hpp"
#include "lm/banner.hpp"

#include "lm/config.hpp"
#include "lm/config/ini.hpp"
#include "lm/config/boot.hpp"

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

    lm::hook::framework_post_parse(config_rw);
    lm::hook::arch_post_parse(config_rw);
    lm::hook::post_parse(config_rw);

    lm::hook::framework_final_config_override(config_rw);

    if(lm::config.test.unit == feature::on)
        lm::hook::test::unit(config_rw);

    lm::hook::framework_main();
    lm::hook::main();
}

auto lm::hook::framework_init([[maybe_unused]] config_t& config) -> void
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
        if(ini_text.data == nullptr || ini_text.size == 0) return;

        auto result = ini::parse(ini_text, config_ini::fields);

        if(result != lm::ini::parse_result::ok) {
            auto re = renum<lm::ini::parse_result>::unqualified(result);
            lm::log::warn("Ini parsed with errors, result=[%.*s]\n", (int)re.size, re.data);
        }
    });
}

auto lm::hook::framework_post_parse([[maybe_unused]] config_t& config) -> void {}

auto lm::hook::framework_final_config_override(config_t& config) -> void
{
    // Override the serials IIF they were defaulted.
    auto uuid = chip::info::uuid();
    if(std::strcmp(config.usb.string_descriptors.serial, "00:00:00:00:00:00") == 0)
    {
        std::snprintf(
            config.usb.string_descriptors.serial,
            config_t::usbcommon::string_descriptor_max_len,
            "%.*s",
            (int)uuid.size, uuid.data
        );
    }
    for(auto i = 0; i < config_t::usbip_t::instance_count; ++i) {
        if(std::strcmp(config.usbip[i].string_descriptors.serial, "00:00:00:00:00:00") == 0)
        {
            std::snprintf(
                config.usbip[i].string_descriptors.serial,
                config_t::usbcommon::string_descriptor_max_len,
                "%.*s",
                (int)uuid.size, uuid.data
            );
        }
    }
}

auto lm::hook::framework_main() -> void
{
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

    // Wait for strandman to be ready.
    while(!fabric::discover_manager())
    {
        lm::log::debug(
            "Waiting for %ums and checking if strandman is up\n",
            lm::config.framework.manager_request_timeout_ms
        );
        fabric::strand::sleep_ms(lm::config.framework.manager_request_timeout_ms);
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
}

[[gnu::weak]] auto lm::hook::init(config_t&) -> void {}
[[gnu::weak]] auto lm::hook::config(config_t&) -> void {}
[[gnu::weak]] auto lm::hook::test::unit(config_t&) -> void {
    log::warn<256>(
        "config.test.unit == on but there was no lm::hook::test::unit linked, this is a most likely a build system problem\n");
}
[[gnu::weak]] auto lm::hook::post_parse(config_t&) -> void {}
[[gnu::weak]] auto lm::hook::main() -> void {}
