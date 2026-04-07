#pragma once

#include "lm/core/types.hpp"

#include "lm/fabric/types.hpp"

namespace lm::config::task
{
    // Most timing-critical task.
    // Must service 1ms SOF interrupts, If this lags, USB disconnects.
    constexpr auto usbd = fabric::task_constants{
        .name = "lm::usbd",
        .priority = 2,
        .stack_size = 64 * 128,
        .core_affinity = 1,
    };

    constexpr auto tud = fabric::task_constants{
        .name = "tinyusb(tud)",
        .priority = 6,
        .stack_size = 64 * 128,
        .core_affinity = 1,
    };

    // ESPNOW / Audio Pump.
    constexpr auto radio = fabric::task_constants{
        .name = "lm::radio",
        .priority = 5,
        .stack_size = 24 * 128,
        .core_affinity = 0,
    };

    constexpr auto tman = fabric::task_constants{
        .name = "lm::tman",
        .priority = 4,
        .stack_size = 26 * 128,
        .core_affinity = 1,
    };

    // Captive portal, MIDI mapping, debouncing, etc.
    constexpr auto app = fabric::task_constants{
        .name = "lm::app",
        .priority = 3,
        .stack_size = 32 * 128,
        .core_affinity = 1,
    };

    // The task that handles flushing the log buffers to their
    // respective sinks (CDC, HID Vendor, ESPNOW).
    constexpr auto logging = fabric::task_constants{
        .name = "lm::logging",
        .priority = 2,
        .stack_size = 24 * 128,
        .core_affinity = 1,
    };

    // General health monitor (RAM, Memory usage, Core usage, etc).
    constexpr auto healthmon = fabric::task_constants{
        .name = "lm::healthmon",
        .priority = 1,
        .stack_size = 22 * 128,
        .core_affinity = 1,
    };

    // Monitors and prints messages posted on the bus.
    constexpr auto busmon = fabric::task_constants{
        .name = "lm::busmon",
        .priority = 1,
        .stack_size = 18 * 128,
        .core_affinity = 1,
    };

    constexpr auto blink = fabric::task_constants{
        .name = "lm::blink",
        .priority = 1,
        .stack_size = 11 * 128,
        .core_affinity = 1,
    };

    // Just so we have something to put in the array below.
    constexpr auto dummy = fabric::task_constants{
        .name = "lm::dummy",
        .priority = 0,
        .stack_size = 0,
        .core_affinity = fabric::task_constants::no_affinity,
    };
}

namespace lm::config::logging
{
    // 4KB log, should be plenty for our usecase.
    constexpr u16 ringbuf_size = 4096;

    // Used internally for printing different messages at different
    // backends (CDC, HID, ESPNOW) simultaneously.
    // If the messages had to be chunked heavily (HID limit is 64bytes per
    // packet, CDC is probably 256, ESPNOW is ~250, ESPNOW2 is ~1400) then
    // this is equal to how many messages ahead the fastest backend can be
    // from the slowest one.
    constexpr u16 consumerbuf_max_size = 32;

    // If you use log() and co. instead of dispatch(), this is the size of the
    // internal buffer for formatting (snprinf), anything longer then
    // this is cutoff before submitted.
    constexpr u16 log_format_bufsize = 128;
}

namespace lm::config::bus
{
    constexpr u16 max_subscribers = 8;
}

namespace lm::config::usb
{
    // The descriptor can get very large when you enable multiple things.
    // This should be able to handle UAC + HID + MIDI (16 cables)
    constexpr u16 config_descriptor_max_size = 128 * 6;
}

namespace lm::config::storage
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
