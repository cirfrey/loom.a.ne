#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <string_view>
#include <algorithm>

#include "lm/version/banner.hpp"

std::string_view clean_val(std::string_view v) {
    auto is_junk = [](unsigned char c) {
        return std::isspace(c) || c == '"' || c == '\'' || c == '\r' || c == '\n';
    };

    auto start = std::find_if_not(v.begin(), v.end(), is_junk);
    auto end = std::find_if_not(v.rbegin(), v.rend(), is_junk).base();

    return (start >= end) ? "" : std::string_view(&*start, std::distance(start, end));
}

auto readf(const char* where) -> std::vector<char>
{
    std::ifstream file(where, std::ios::binary | std::ios::ate);
    if (!file) {
        std::fprintf(stderr, "Error: Could not open file %s\n", where);
        return {};
    }
    auto fsize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(fsize);
    file.read(buffer.data(), fsize);
    return buffer;
}

std::ofstream out;
int main(int argc, char** argv) {
    using namespace lm;

    if (argc < 5) {
        std::fprintf(stderr, "Usage: %s <outfile> <banner_defs.ini> <chip_name.hpp.inc> <banner.hpp.inc>\n", argv[0]);
        return 1;
    }

    auto buffer = readf(argv[2]);
    if(buffer.size() == 0) return 1;

    version::banner_data data;

    std::string_view content(buffer.data(), buffer.size());
    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\n', pos);
        std::string_view line = content.substr(pos, (end == std::string_view::npos) ? content.size() - pos : end - pos);
        pos = (end == std::string_view::npos) ? content.size() : end + 1;

        auto sep = line.find('=');
        if (sep == std::string_view::npos) continue;

        auto key = clean_val(line.substr(0, sep));
        auto val = clean_val(line.substr(sep + 1));

        if (key.empty() || val.empty()) continue;

        if (key == "version_major")      data.ver_major = (u8)std::atoi(std::string(val).c_str());
        else if (key == "version_minor") data.ver_minor = (u8)std::atoi(std::string(val).c_str());
        else if (key == "git_hash")      data.git_hash = { val.data(), (st)val.size() };
        else if (key == "board")         data.board = { val.data(), (st)val.size() };
        else if (key == "arch")          data.arch = { val.data(), (st)val.size() };
        else if (key == "build_date")    data.build_date = { val.data(), (st)val.size() };
        else if (key == "max_ram")       data.total_ram = (st)std::atoll(std::string(val).c_str());

        std::fprintf(stderr, "Found: %.*s = %.*s\n", (int)key.size(), key.data(), (int)val.size(), val.data());
    }

    std::fprintf(stderr, "\n--- [Internal Data Review] ---\n");
    std::fprintf(stderr, "Version    : v%u.%u\n", data.ver_major, data.ver_minor);
    std::fprintf(stderr, "Arch       : %.*s\n", (int)data.arch.size, data.arch.data);
    std::fprintf(stderr, "Board      : %.*s\n", (int)data.board.size, data.board.data);
    std::fprintf(stderr, "Git Hash   : %.*s\n", (int)data.git_hash.size, data.git_hash.data);
    std::fprintf(stderr, "Build Date : %.*s\n", (int)data.build_date.size, data.build_date.data);
    std::fprintf(stderr, "Total RAM  : %zu bytes\n", data.total_ram);
    std::fprintf(stderr, "------------------------------\n\n");

    auto chip_name = readf(argv[3]);
    if(chip_name.size()) data.chip_name = chip_name.data() | to_text;
    auto banner = readf(argv[4]);
    if(banner.size()) data.banner = banner.data() | to_text;

    out = std::ofstream(argv[1], std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "Error: Could not open output file %s\n", argv[2]);
        return 1;
    }

    version::write_banner(
        [](text t) { out.write(t.data, (std::streamsize)t.size); },
        nullptr,
        {"", 0},
        data
    );
    return 0;
}
