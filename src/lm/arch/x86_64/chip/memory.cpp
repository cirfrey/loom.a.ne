#include "lm/chip/all.hpp"
#include "lm/core/all.hpp"

#include <cstring>

auto lm::chip::memory::total() -> st { return 16 * 1024 * 1024; } // Mock 16MB
auto lm::chip::memory::free() -> st { return 10 * 1024 * 1024; }  // Mock 10MB
auto lm::chip::memory::peak_used() -> st { return 2 * 1024 * 1024; } // Mock 2MB
auto lm::chip::memory::largest_free_block() -> st { return 8 * 1024 * 1024; }

auto lm::chip::memory::storage::read(st offset, mut_buf out) const -> storage_op_status
{
    if (offset + out.size > size) return storage_op_status::out_of_bounds;

    auto* simulated_flash = static_cast<const u8*>(impl);
    std::memcpy(out.data, &simulated_flash[offset], out.size);
    return storage_op_status::ok;
}

auto lm::chip::memory::storage::write(st offset, buf data) -> storage_op_status
{
    if (readonly) return storage_op_status::unauthorized_access;
    if (offset + data.size > size) return storage_op_status::out_of_bounds;

    auto* simulated_flash = static_cast<u8*>(impl);

    // SPI Flash simulation: Can only turn 1s into 0s.
    // A real strict simulator would do: simulated_flash[i] &= data[i]
    std::memcpy(&simulated_flash[offset], data.data, data.size);
    return storage_op_status::ok;
}

auto lm::chip::memory::storage::erase(st offset, st length) -> storage_op_status
{
    if (readonly) return storage_op_status::unauthorized_access;
    if (offset + length > size) return storage_op_status::out_of_bounds;

    // Strict alignment guard remains, hardware rules still apply in the simulator
    if ((offset % sector_size != 0) || (length % sector_size != 0)) {
        return storage_op_status::unaligned_access;
    }

    auto* simulated_flash = static_cast<u8*>(impl);
    // Erasing flash hardware resets bytes to 0xFF
    std::memset(&simulated_flash[offset], 0xFF, length);
    return storage_op_status::ok;
}

auto lm::chip::memory::get_storages() -> std::span<storage>
{
    // Simulate your 1MB "static" FAT/MSC partition
    constexpr st static_part_size = 1024 * 1024;
    static u8 sim_flash_static[static_part_size];
    static bool is_init = false;

    static storage mock_storages[] = {
        {
            .label         = "static" | to<char const*> | to_text,
            .type          = 0x01,
            .subtype       = 0x81,
            .size          = static_part_size,
            .sector_size   = 4096,
            .absolute_base = 0x10000,
            .readonly      = false,
            .impl          = sim_flash_static,
        }
    };

    if (!is_init) {
        // Flash is 0xFF when erased, not 0x00
        std::memset(sim_flash_static, 0xFF, static_part_size);
        is_init = true;
    }

    return {mock_storages, 1};
}
