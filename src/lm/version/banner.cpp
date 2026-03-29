#include "lm/version/banner.hpp"

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
auto lm::version::write_banner_to_console(u8 major, u8 minor, char const* git_hash, char const* build_date, char const* prefix, u8 interval) -> void
{
    auto write = [&](char const* buf, u32 len){
        if(!interval) {
            lm::board::console_write(buf, len);
            return;
        }

        while(len--) {
            lm::board::console_write(buf++, 1);
            lm::task::delay_ms(interval);
        }
    };

    // --- Prefix ---
    write(prefix, strlen(prefix));

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
    static char const default_banner_art[] = R"(
>       //      //      //      //      //    //      //      //      /
>      //      //      //      //      //    //      //      //      //
>     //  /\  //  /\  //  /\  //  /\  //    //  /\  //  /\  //  /\  //
>    //  /  \//  /  \//  /  \//  /  \//    //  /  \//  /  \//  /  \//
>   //  /    /  /    /  /    /  /    /    //  /    /  /    /  /    /  /
>  //  /    /  /    /  /    /  /    /    //  /    /  /    /  /    /  /
> //  /    // /   //  /   //  /   //    /   //   /  //   /  //   /  /
)";
    auto art = lm::board::get_banner_art();
    if(!art.data || !art.len)
    art = { .data = default_banner_art + 1, .len = sizeof(default_banner_art) - 2 }; // Skip the first newline and the '\0'
    write(art.data, art.len);

    // --- Footer ---
    char chip[36];
    std::snprintf(chip, 36, "%s", lm::board::get_pretty_name());
    uint32_t total_ram = lm::board::get_ram() / 1024;
    uint32_t free_ram  = lm::board::get_free_ram() / 1024;
    uint32_t used_ram  = total_ram - free_ram;
    char uptime[64];
    lm::format_uptime(lm::board::get_uptime(), uptime, 64);

    char footer[320];
    auto footer_size = std::snprintf(
        footer, 320,
        "> ------------------------------------------------- [%s]\n"
        ">  [CHIP  ] %-36s [RAM ] %ukb / %ukb\n"
        ">  [UPTIME] %-36s [TEMP] %.1f°C\n"
        "> --------------------------------------- [%s:%s]\n",
        lm::board::get_mac_str(),
        chip, used_ram, total_ram,
        uptime, lm::board::get_cpu_temperature(),
        git_hash, build_date
    );
    write(footer, footer_size);
}
