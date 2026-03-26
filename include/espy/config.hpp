#pragma once

#include "espy/aliases.hpp"

namespace espy::config::task
{
    // Used for identifying the sender
    // in the bus and maybe other places.
    enum class id_t
    {
        unspecified = 0,
        usbd,
        radio,
        app,
        logging,
        healthmon,
        blink
    };

    struct info_t
    {
        enum class affinity_t {
            core_wifi,
            core_processing,
            no_affinity,
        };

        id_t id;
        u8 priority;
        const char* name;
        u16 stack_size;
        u16 sleep_ms; // How much it should sleep between loops, or however it may sleep internally, if applicable.
        u16 bus_size; // Size for the queue for the bus, when applicable.
        affinity_t core_affinity; // What core it should run at, remember that the ESP32S2 has 1 core and the ESP32S3 has 2.
    };

    // Most timing-critical task.
    // Must service 1ms SOF interrupts, If this lags, USB disconnects.
    constexpr auto usb_device = info_t{
        .id = id_t::usbd,
        .priority = 5,
        .name = "espy::usb",
        .stack_size = 8192,
        .sleep_ms = 1,
        .bus_size = 0,
        .core_affinity = info_t::affinity_t::core_processing,
    };

    // ESPNOW / Audio Pump.
    constexpr auto radio = info_t{
        .id = id_t::radio,
        .priority = 4,
        .name = "espy::radio",
        .stack_size = 3072,
        .sleep_ms = 1,
        .bus_size = 0,
        .core_affinity = info_t::affinity_t::core_wifi,
    };

    // Captive portal, MIDI mapping, debouncing, etc.
    constexpr auto app = info_t{
        .id = id_t::app,
        .priority = 3,
        .name = "espy::app",
        .stack_size = 4096,
        .sleep_ms = 1,
        .bus_size = 0,
        .core_affinity = info_t::affinity_t::core_processing,
    };

    // The task that handles flushing the log buffers to their
    // respective sinks (CDC, HID Vendor, ESPNOW).
    constexpr auto logging = info_t{
        .id = id_t::logging,
        .priority = 2,
        .name = "espy::usb::logging",
        .stack_size = 3072,
        .sleep_ms = 1,
        .bus_size = 8,
        .core_affinity = info_t::affinity_t::core_processing,
    };

    // General health monitor (RAM, Memory usage, Core usage, etc).
    constexpr auto healthmon = info_t{
        .id = id_t::healthmon,
        .priority = 1,
        .name = "espy::healthmon",
        .stack_size = 2048,
        .sleep_ms = 1000,
        .core_affinity = info_t::affinity_t::core_processing,
    };

    constexpr auto blink = info_t{
        .id = id_t::blink,
        .priority = 1,
        .name = "espy::blink",
        .stack_size = 2048,
        .sleep_ms = 10,
        .core_affinity = info_t::affinity_t::core_processing,
    };
}

namespace espy::config::logging
{
    // 4KB log, should be plenty for our usecase.
    constexpr u16 ringbuf_size = 4096;

    // Used internally for printing different messages at different
    // backends (CDC, HID, ESPNOW) simultaneously.
    // If the messages had to be chunked heavily (HID limit is 64bytes per
    // packet, CDC is probably 256, ESPNOW is ~250, ESPNOW2 is ~1400) then
    // this is equal to how many messages ahead the fastest backend can be
    // from the slowest one.
    constexpr u16 dispatchbuf_max_size = 32;

    // If you use logf() instead of log(), this is the size of the
    // internal buffer for formatting (vsnprinf), anything longer then
    // this is cutoff before submitted.
    constexpr u16 logf_bufsize = 256;
}

namespace espy::config::bus
{
    constexpr u16 max_subscribers = 8;
}

namespace espy::config::usb
{
    // The descriptor can get very large when you enable multiple things.
    // This should be able to handle UAC + HID + MIDI (16 cables)
    constexpr u16 config_descriptor_max_size = 128 * 6;
}

namespace espy::config::storage
{
    struct info_t {
        const char* mount_point;
        const char* partition_name; // Should correspond to what is set on partitions.csv.
        u8 lun;
    };

    constexpr auto static_data = info_t{
        .mount_point = "/static",
        .partition_name = "static",
        .lun = 0,
    };
}
