#include "lm/version/banner.hpp"

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
    writer_t writer,
    sleeper_t sleeper,
    text prefix,
    banner_data data
) -> void
{
    auto write = [&](char const* data, u32 size){
        if(!sleeper) {
            writer({data, size});
            return;
        }

        while(size--) {
            writer({data++, 1});
            sleeper();
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
        data.ver_major, data.ver_minor,
        get_version_adjective(data.ver_major, data.ver_minor),
        get_version_subject(data.ver_major, data.ver_minor)
    );
    // Copy it to the end of the header (but don't overwrite the '\n').
    std::memcpy(header + headerlen - versionlen - 2, version, versionlen);

    write(header, headerlen - 1);

    // --- Art ---
    static constexpr text default_banner = R"(
>       //      //      //      //      //    //      //      //      /
>      //      //      //      //      //    //      //      //      //
>     //  /\  //  /\  //  /\  //  /\  //    //  /\  //  /\  //  /\  //
>    //  /  \//  /  \//  /  \//  /  \//    //  /  \//  /  \//  /  \//
>   //  /    /  /    /  /    /  /    /    //  /    /  /    /  /    /  /
>  //  /    /  /    /  /    /  /    /    //  /    /  /    /  /    /  /
> //  /    // /   //  /   //  /   //    /   //   /  //   /  //   /  /
)" | to_text | trans([](auto b){ return text{b.data+1, b.size-1}; }); // Drop the first '\n'
    auto art = data.banner;
    if(!art.data || !art.size) art = default_banner;
    write(art.data, art.size);

    // --- Footer ---
    char chip[36];
    auto full_name = data.chip_name;
    std::snprintf(chip, sizeof(chip), "%.*s", (int)full_name.size, full_name.data);
    char uptime[64];
    lm::format_uptime(data.uptime_us, uptime, sizeof(uptime));

    char footer[320];
    auto footer_size = std::snprintf(
        footer, sizeof(footer),
        "> ------------------------------------------------- [%.*s]\n"
        ">  [CHIP  ] %-36s [RAM ] %lukb / %lukb\n"
        ">  [UPTIME] %-36s [TEMP] %.1fC\n",
        (int)data.uuid.size, data.uuid.data,
        chip, data.total_ram - data.free_ram, data.total_ram,
        uptime, data.temp
    );
    write(footer, footer_size);

    char final_line[100];
    auto final_line_size = std::snprintf(
        final_line, sizeof(final_line),
        "> --------------------------------------- [%.*s:%.*s]\n",
        (int)data.git_hash.size, data.git_hash.data,
        (int)data.build_date.size, data.build_date.data
    );
    auto archboard_size = std::snprintf(
        &final_line[2], 32,
        "[%.*s:%.*s] ",
        (int)data.arch.size, data.arch.data,
        (int)data.board.size, data.board.data
    );
    final_line[archboard_size + 2] = '-'; // Override the '\0'
    write(final_line, final_line_size);
}
