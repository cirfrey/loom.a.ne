#include "lm/hooks.hpp"

#include "lm/board.hpp"
#include "lm/chip.hpp"
#include "lm/core.hpp"
#include "lm/ini.hpp"
#include "lm/log.hpp"

#include "lm/arch/x86_64/endpoints.hpp"
#include "lm/arch/x86_64/program_args.hpp"
#include "lm/strands/usbip.hpp"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>

auto lm::hook::arch_init([[maybe_unused]] config_t& config) -> void
{
    chip::system::init();
    chip::uart::init(board::uart_trace, board::gpio_uart_trace_tx, board::gpio_uart_trace_rx);
}


// These fields are specific to lm::hook::arch_config.
namespace lm::arch_config
{
    char inipath[128] = "";
    st   inipath_size = 0;

    ini::field fields[] = {
        ini::str("native.inipath"_text, inipath, {
            .max_len = sizeof(inipath),
            .size_out = [](u8, st v){ inipath_size = v; },
        }),
    };
}

#include "lm/config/ini.hpp"

auto lm::hook::arch_config(config_t& config) -> void
{
    // Set arch defaults.
    config.test.unit = feature::off;

    // Override all fields that are *ours*.
    for(auto& f : config_ini::fields) {
        for(auto& my_f : lm::arch_config::fields) {
            if(f.key == my_f.key) f = my_f;
        }
    }

    text allowlist[] = { "native.inipath"_text };
    if(arch::x86_64::program_args.size() > 1)
        log::debug("Seeding config with specific arguments from command line\n");
    // Tries to parse the fields from the command line (since this is before lm::hook::parse_ini() they will be
    // overriden again but we will further override them yet again in lm::hook::arch_post_parse()).
    for(st i = 1; i < arch::x86_64::program_args.size(); ++i)
    {
        auto result = ini::parse(arch::x86_64::program_args[i] | to_text, config_ini::fields, {}, allowlist);

        if(result != lm::ini::parse_result::ok) {
            auto re = renum<lm::ini::parse_result>::unqualified(result);
            log::warn("Ini parsed with errors, result=[%.*s]\n", (int)re.size, re.data);
        }
    }

    // TODO: Im not happy with this, the spans allow write.
    //       Ideally we'd do span<type const> and then each
    //       backend creates its own copy from the template.
    // NOTE: Cannot have multiple usbip instances until the previous commented is implemented.
    config.usb.endpoints    = arch::x86_64::endpoints;
    for(auto i = 0; i < config_t::usbip_t::instance_count; ++i)
        config.usbip[i].endpoints  = lm::strands::usbip_backend::endpoints;

    config.ini.with_source = [](void* ud, auto cb){
        if(lm::arch_config::inipath_size == 0){
            log::debug("No native.inipath set, will forward empty string to config.ini.with_source callback\n");
            cb(ud, {nullptr, 0});
            return;
        }

        std::error_code ec;
        auto file_size = std::filesystem::file_size(lm::arch_config::inipath, ec) | to<st>;
        if(ec)
        {
            log::warn<256>(
                "Error reading file [%.*s], will forward empty string to config.ini.with_source callback\n> Err: %s\n",
                (int)lm::arch_config::inipath_size,
                lm::arch_config::inipath,
                ec.message().c_str()
            );
            cb(ud, {nullptr, 0});
            return;
        }

        std::ifstream file(lm::arch_config::inipath, std::ios::binary);
        if(!file.is_open()) {
            log::warn("Failed to open [%.*s], forwarding empty string\n",
                (int)lm::arch_config::inipath_size,
                lm::arch_config::inipath
            );
            cb(ud, {nullptr, 0});
            return;
        }

        std::vector<char> buffer(file_size);
        auto const n = static_cast<st>(file.read(buffer.data(), file_size).gcount());
        if(n != file_size) {
            log::warn("Truncated read on [%.*s] (%lu/%lu bytes), skipping\n",
                (int)lm::arch_config::inipath_size, lm::arch_config::inipath,
                n, file_size);
            cb(ud, {nullptr, 0});
            return;
        }

        log::debug(
            "Forwarding [%.*s] to config.ini.with_source callback\n",
            (int)lm::arch_config::inipath_size,
            lm::arch_config::inipath
        );
        cb(ud, text{buffer.data(), static_cast<st>(n)});
    };
}

auto lm::hook::arch_post_parse([[maybe_unused]] config_t& config) -> void
{
    // Override all config fields with stuff passed via command line.
    text denylist[] = { "native.inipath"_text };
    if(arch::x86_64::program_args.size() > 1)
        log::debug("Overriding config with arguments from command line\n");
    for(st i = 1; i < arch::x86_64::program_args.size(); ++i)
    {
        auto result = ini::parse(arch::x86_64::program_args[i] | to_text, config_ini::fields, {}, {}, denylist);

        if(result != lm::ini::parse_result::ok) {
            auto re = renum<lm::ini::parse_result>::unqualified(result);
            log::warn("Ini parsed with errors, result=[%.*s]\n", (int)re.size, re.data);
        }
    }
}
