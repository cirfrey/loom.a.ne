#include "lm/hooks.hpp"

#include "lm/board.hpp"
#include "lm/chip.hpp"
#include "lm/log.hpp"
#include "lm/fabric.hpp"
#include "lm/build.hpp"
#include "lm/banner.hpp"

auto lm::hook::arch_init(config_t& config) -> void
{
    chip::system::init();
    chip::uart::init(board::uart_trace, board::gpio_uart_trace_tx, board::gpio_uart_trace_rx);

    // By printing the banner we make sure that things like the mac address are
    // initialized and can be used by the rest of the code.
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
}



#include "lm/arch/x86_64/endpoints.hpp"
#include "lm/strands/usbip.hpp"

#include <cstdio>

auto lm::hook::arch_config(config_t& config) -> void
{
    auto uuid = chip::info::uuid();
    std::snprintf(
        config.usb.string_descriptors.serial,
        config_t::usbcommon::string_descriptor_max_len,
        "%.*s",
        (int)uuid.size, uuid.data
    );
    std::snprintf(
        config.usbip.string_descriptors.serial,
        config_t::usbcommon::string_descriptor_max_len,
        "%.*s",
        (int)uuid.size, uuid.data
    );

    // TODO: Im not happy with this, the spans allow write.
    //       Ideally we'd do span<type const> and then each
    //       backend creates its own copy from the template.
    // NOTE: Cannot have multiple usbip instances until the previous commented is implemented.
    config.usb.endpoints    = arch::x86_64::endpoints;
    config.usbip.endpoints  = lm::strands::usbip_backend::endpoints;

    // TODO: Hardcoded for now, until I actually fetch the ini file from somewhere.
    config.ini.with_source = [](void* ud, auto cb){
        auto ini_text = "[midi.usbip]\ncable_count = 4"_text;
        cb(ud, ini_text);
    };
}
