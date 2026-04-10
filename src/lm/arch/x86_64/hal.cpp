#include "lm/chip.hpp"
#include "lm/core.hpp"

#include <chrono>
#include <thread>
#include <cstring>
#include <cstdio>
#include <cstdlib>

/* --- chip::info --- */

auto lm::chip::info::codename() -> text
{
    return "Bobby" | to_text;
}

auto lm::chip::info::name() -> text
{
    #include "lm/arch/x86_64/name.hpp"
    return {name, sizeof(name)};
}

auto lm::chip::info::uuid() -> text
{
    // Dummy MAC address for native host
    static char mac_str[] = "ca:fe:de:ad:be:ef";
    return { .data = mac_str, .size = sizeof(mac_str) - 1 };
}

auto lm::chip::info::banner() -> text
{
    #include "lm/arch/x86_64/banner.hpp"
    return {banner, sizeof(banner)};
}

/* --- chip::system --- */

auto lm::chip::system::init() -> void
{
    // No-op for native OS
}

auto lm::chip::system::reboot() -> void
{
    std::exit(0);
}

auto lm::chip::system::core_count() -> st
{
    st cores = std::thread::hardware_concurrency();
    return cores > 0 ? cores : 1;
}

/* --- chip::time --- */

auto lm::chip::time::uptime() -> u64
{
    static auto boot_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - boot_time).count();
}

/* --- chip::gpio --- */

namespace lm::chip::gpio {
    // Mock registers to hold the state of up to 40 "pins"
    static bool pin_states[40] = {false};
}

auto lm::chip::gpio::init(pin p, pin_mode mode) -> void
{
    // Just bounds check on native to prevent segfaults
    if (p < 40) pin_states[p] = false;
}

auto lm::chip::gpio::get(pin p) -> bool
{
    return (p < 40) ? pin_states[p] : false;
}

auto lm::chip::gpio::set(pin p, bool level) -> void
{
    if (p < 40) pin_states[p] = level;
}

auto lm::chip::gpio::attach_interrupt(pin p, interrupt_edge edge, isr_handler handler, void* arg) -> void
{
    // On native, hardware interrupts don't exist.
    // You would typically trigger these manually via a test harness or an OS signal.
}

auto lm::chip::gpio::detach_interrupt(pin p) -> void {}

/* --- chip::uart --- */

auto lm::chip::uart::port_count() -> uart_port { return 1; }
auto lm::chip::uart::max_port()   -> uart_port { return 0; }

auto lm::chip::uart::init(uart_port port, pin tx, pin rx, st baud_rate) -> void {}

auto lm::chip::uart::write(uart_port port, buf data) -> st
{
    // Route UART directly to stdout for the native console
    auto len = std::fwrite(data.data, 1, data.size, stdout);
    std::fflush(stdout);
    return len;
}

/* --- chip::memory --- */

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

/* --- chip::sensor --- */

auto lm::chip::sensor::internal_temperature() -> f32
{
    // Desktop CPUs run hot; here's a warm mock value
    return 45.5f;
}

/* --- chip::usb --- */

auto lm::chip::usb::phy::power_up() -> void
{
    // No-op. TinyUSB Native port usually hooks directly into the host OS networking/USB stack
    // or runs via an emulation layer without needing raw PHY config.
}
