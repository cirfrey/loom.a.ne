#pragma once

#include "lm/core/types.hpp"

#include "lm/task/types.hpp"

namespace lm::config::task
{
    // Most timing-critical task.
    // Must service 1ms SOF interrupts, If this lags, USB disconnects.
    constexpr auto usbd = lm::task::config{
        .id = lm::task::id_t::usbd,
        .priority = 6,
        .name = "lm::usbd",
        .stack_size = 8192,
        .sleep_ms = 1,
        .core_affinity = 1,
    };

    // ESPNOW / Audio Pump.
    constexpr auto radio = lm::task::config{
        .id = lm::task::id_t::radio,
        .priority = 5,
        .name = "lm::radio",
        .stack_size = 3072,
        .sleep_ms = 1,
        .core_affinity = 0,
    };

    constexpr auto sysman = lm::task::config{
        .id = lm::task::id_t::sysman,
        .priority = 4,
        .name = "lm::sysman",
        .stack_size = 4096,
        .sleep_ms = 10,
        .core_affinity = 1,
    };

    // Captive portal, MIDI mapping, debouncing, etc.
    constexpr auto app = lm::task::config{
        .id = lm::task::id_t::app,
        .priority = 3,
        .name = "lm::app",
        .stack_size = 4096,
        .sleep_ms = 1,
        .core_affinity = 1,
    };

    // The task that handles flushing the log buffers to their
    // respective sinks (CDC, HID Vendor, ESPNOW).
    constexpr auto logging = lm::task::config{
        .id = lm::task::id_t::logging,
        .priority = 2,
        .name = "lm::logging",
        .stack_size = 3072,
        .sleep_ms = 1,
        .core_affinity = 1,
    };

    // General health monitor (RAM, Memory usage, Core usage, etc).
    constexpr auto healthmon = lm::task::config{
        .id = lm::task::id_t::healthmon,
        .priority = 1,
        .name = "lm::healthmon",
        .stack_size = 4096,
        .sleep_ms = 1000,
        .core_affinity = 1,
    };

    // Monitors and prints messages posted on the bus.
    constexpr auto busmon = lm::task::config{
        .id = lm::task::id_t::busmon,
        .priority = 1,
        .name = "lm::busmon",
        .stack_size = 4096,
        .sleep_ms = 10,
        .core_affinity = 1,
    };

    constexpr auto blink = lm::task::config{
        .id = lm::task::id_t::blink,
        .priority = 1,
        .name = "lm::blink",
        .stack_size = 2048,
        .sleep_ms = 10,
        .core_affinity = 1,
    };

    // Just so we have something to put in the array below.
    constexpr auto dummy = lm::task::config{
        .id = lm::task::id_t::unspecified,
        .priority = 0,
        .name = "lm::dummy",
        .stack_size = 0,
        .sleep_ms = 1000 * 32,
        .core_affinity = lm::task::config::no_affinity,
    };

    constexpr lm::task::config by_id[] = {
        [(u8)lm::task::id_t::unspecified] = dummy,
        [(u8)lm::task::id_t::app] = app,
        [(u8)lm::task::id_t::blink] = blink,
        [(u8)lm::task::id_t::busmon] = busmon,
        [(u8)lm::task::id_t::healthmon] = healthmon,
        [(u8)lm::task::id_t::logging] = logging,
        [(u8)lm::task::id_t::radio] = radio,
        [(u8)lm::task::id_t::sysman] = sysman,
        [(u8)lm::task::id_t::usbd] = usbd,
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
    constexpr u16 dispatchbuf_max_size = 32;

    // If you use logf() instead of log(), this is the size of the
    // internal buffer for formatting (vsnprinf), anything longer then
    // this is cutoff before submitted.
    constexpr u16 logf_bufsize = 256;
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
