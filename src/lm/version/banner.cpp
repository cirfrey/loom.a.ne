#include "lm/version/banner.hpp"

#include "lm/chip/info.hpp"
#include "lm/chip/uart.hpp"
#include "lm/chip/memory.hpp"
#include "lm/chip/time.hpp"
#include "lm/chip/sensor.hpp"
#include "lm/board.hpp"
#include "lm/task.hpp"

#include <cstdio>
#include <cstring>
#include <cinttypes>

auto lm::version::get_version_adjective(u8 major, u8 minor) -> char const* { return adjectives[(major + minor) % 32]; }
auto lm::version::get_version_subject(u8 major, u8 minor)   -> char const* { return subjects[(minor + major * 13) % 32]; }

namespace lm
{
    auto format_uptime(u64 micros, char* buf, u32 size) -> int
    {
        u64 secs_tot = micros / 1000000;
        u8  s = secs_tot % 60;
        u8  m = (secs_tot / 60) % 60;
        u8  h = (secs_tot / 3600) % 24;
        u64 d = (secs_tot / 86400);

        auto d_str = d == 1 ? "day"    : "days";
        auto h_str = h == 1 ? "hour"   : "hours";
        auto m_str = m == 1 ? "minute" : "minutes";
        auto s_str = s == 1 ? "second" : "seconds";

        if (d > 0)      return std::snprintf(buf, size, "%" PRIu64 " %s, %" PRIu8 " %s, %" PRIu8 " %s", d, d_str, h, h_str, m, m_str);
        else if (h > 0) return std::snprintf(buf, size, "%" PRIu8 " %s, %" PRIu8 " %s, %" PRIu8 " %s", h, h_str, m, m_str, s, s_str);
        else if (m > 0) return std::snprintf(buf, size, "%" PRIu8 " %s, %" PRIu8 " %s", m, m_str, s, s_str);
        else            return std::snprintf(buf, size, "%" PRIu8 " %s", s, s_str);
    }
}

// The code is split up by printing the prefix -> header -> art -> footer.
auto lm::version::write_banner(
    chip::uart_port port,
    u8 major, u8 minor,
    text git_hash, text build_date,
    text prefix,
    u8 interval
) -> void
{
    auto write = [&](char const* data, u32 size){
        if(!interval) {
            chip::uart::write(port, {data, size});
            return;
        }

        while(size--) {
            chip::uart::write(port, {data++, 1});
            lm::task::sleep_ms(interval);
        }
    };

    // --- Prefix ---
    write(prefix.data, prefix.size);

    // --- Header ---
    const auto headerlen = 73;
    char header[headerlen] = "> ---------------------------------------------------------------------\n";

    char version[64];
    auto versionlen = std::snprintf(
        version, 64,
        " Loom Audio Nexus (loom.a.ne) v%u.%u - %s %s",
        major, minor,
        get_version_adjective(major, minor),  get_version_subject(major, minor)
    );
    // Copy it to the end of the header (but don't overwrite the '\n').
    std::memcpy(header + headerlen - versionlen - 2, version, versionlen);

    write(header, headerlen - 1);

    // --- Art ---
    static constexpr text default_banner = text::from(R"(
>       //      //      //      //      //    //      //      //      /
>      //      //      //      //      //    //      //      //      //
>     //  /\  //  /\  //  /\  //  /\  //    //  /\  //  /\  //  /\  //
>    //  /  \//  /  \//  /  \//  /  \//    //  /  \//  /  \//  /  \//
>   //  /    /  /    /  /    /  /    /    //  /    /  /    /  /    /  /
>  //  /    /  /    /  /    /  /    /    //  /    /  /    /  /    /  /
> //  /    // /   //  /   //  /   //    /   //   /  //   /  //   /  /
)");
    auto art = chip::info::banner();
    if(!art.data || !art.size) art = {
        .data = default_banner.data + 1,
        .size = default_banner.size - 1
    };
    write(art.data, art.size);

    // --- Footer ---
    auto uuid = chip::info::uuid();
    char chip[36];
    std::snprintf(chip, 36, "%s", chip::info::full_name());
    uint32_t total_ram = chip::memory::total() / 1024;
    uint32_t free_ram  = chip::memory::free() / 1024;
    uint32_t used_ram  = total_ram - free_ram;
    char uptime[64];
    lm::format_uptime(chip::time::uptime(), uptime, 64);

    char footer[320];
    auto footer_size = std::snprintf(
        footer, 320,
        "> ------------------------------------------------- [%.*s]\n"
        ">  [CHIP  ] %-36s [RAM ] %ukb / %ukb\n"
        ">  [UPTIME] %-36s [TEMP] %.1f°C\n"
        "> --------------------------------------- [%.*s:%.*s]\n",
        uuid.size, uuid.data,
        chip, used_ram, total_ram,
        uptime, chip::sensor::internal_temperature(),
        git_hash.size, git_hash.data, build_date.size, build_date.data
    );
    write(footer, footer_size);
}
