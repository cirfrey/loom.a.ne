#pragma once

#include "lm/core/types.hpp"
#include "lm/core/ansi.hpp"
#include "lm/fabric/types.hpp"

// TODO: #include "loomane_config.h"
//       user overrides.

namespace lm
{
    // The config object.
    // It has:
    // - Static fields (memory allocation stuff, or things that must be known
    //   at compile time) customizable by macros and;
    // - Dynamic fields (runtime stuff) customizable by regular assignment.
    struct config_t
    {
        // Use this instead of bools, it'll the intent in code more clear.
        enum class feature : u8
        {
            disabled = 0, off = 0, no  = 0,
            enabled = 1,  on = 1,  yes = 1,
        };
        // The ini-facing version, use this for parsing and normalize into config_t::feature.
        // If we don't do this the end-user can only use [off] and [on], and the end-user is king so...
        // NOTE: If modifying, keep the [0, 1] in the first two fields,
        //       otherwise end-user will get nonsense output during parsing.
        // NOTE: it's very important both of the enums have the same size and alignment.
        enum class feature_ini : u8
        {
            disabled, enabled,
            no, yes,
            off, on,
        };
        static constexpr auto normalize_feature_ini(feature_ini f) -> feature
        {
            if(
                f == feature_ini::off ||
                f == feature_ini::disabled ||
                f == feature_ini::no
            ) return feature::off;
            return feature::on;
        }

        struct logging_t
        {
            // 4KB log, should be plenty for our usecase.
            #ifndef LM_CONFIG_LOGGING_RINGBUF_SIZE
            #define LM_CONFIG_LOGGING_RINGBUF_SIZE 4096
            #endif
            static constexpr u16 ringbuf_size = LM_CONFIG_LOGGING_RINGBUF_SIZE;

            // Used internally for printing different messages at different
            // backends (CDC, HID, ESPNOW) simultaneously.
            // If the messages had to be chunked heavily (HID limit is 64bytes per
            // packet, CDC is probably 256, ESPNOW is ~250, ESPNOW2 is ~1400) then
            // this is equal to how many messages ahead the fastest backend can be
            // from the slowest one.
            #ifndef LM_CONFIG_LOGGING_CONSUMERBUF_MAX_SIZE
            #define LM_CONFIG_LOGGING_CONSUMERBUF_MAX_SIZE 32
            #endif
            static constexpr u16 consumerbuf_max_size = LM_CONFIG_LOGGING_CONSUMERBUF_MAX_SIZE;

            // If you use log() and co. instead of dispatch(), this is the size of the
            // internal buffer for formatting (snprinf), anything longer then
            // this is cutoff before submitted.
            #ifndef LM_CONFIG_LOGGING_FORMAT_BUFSIZE
            #define LM_CONFIG_LOGGING_FORMAT_BUFSIZE 128
            #endif
            static constexpr u16 format_bufsize = LM_CONFIG_LOGGING_FORMAT_BUFSIZE;

            // Log level customization.
            enum level {
                debug,
                info,
                regular,
                warn,
                error,
                #ifdef LM_CONFIG_LOGGING_EXTRA_SEVERITIES
                    LM_CONFIG_LOGGING_EXTRA_SEVERITIES
                #endif

                level_count
            };

            feature level_enabled[level_count] = {
                [debug]   = feature::on,
                [info]    = feature::on,
                [regular] = feature::on,
                [warn]    = feature::on,
                [error]   = feature::on,
                #ifdef LM_CONFIG_LOGGING_EXTRA_SEVERITIES_ENABLED
                    LM_CONFIG_LOGGING_EXTRA_SEVERITIES_ENABLED
                #endif
            };


            // Overridable log color.
            text level_ansi[level_count] = {
                [debug]   = ansi::code<ansi::fg::gray>,
                [info]    = ansi::code<ansi::fg::white>,
                [regular] = ansi::code<ansi::style::reset>,
                [warn]    = ansi::code<ansi::fg::yellow>,
                [error]   = ansi::code<ansi::fg::red>,
                #ifdef LM_CONFIG_LOGGING_EXTRA_SEVERITIES_ANSI
                    LM_CONFIG_LOGGING_EXTRA_SEVERITIES_ANSI
                #endif
            };

            // TODO: level prefix.

            // Also able disable logging in general.
            feature toggle = feature::on;
            // Or override with your own function.
            using dispatcher_t = bool(*)(text);
            dispatcher_t custom_dispatcher = nullptr;
        } logging;

        struct bus_t
        {
            #ifndef LM_CONFIG_BUS_MAX_SUBSCRIBERS
            #define LM_CONFIG_BUS_MAX_SUBSCRIBERS 16
            #endif
            static constexpr u16 max_subscribers = LM_CONFIG_BUS_MAX_SUBSCRIBERS;
        } bus;

        struct usb_t
        {
            #ifndef LM_CONFIG_USB_CONFIG_DESCRIPTOR_MAX_SIZE
            #define LM_CONFIG_USB_CONFIG_DESCRIPTOR_MAX_SIZE (128 * 6)
            #endif
            // The descriptor can get very large when you enable multiple things.
            // This should be able to handle UAC + HID + MIDI (16 cables)
            static constexpr u16 config_descriptor_max_size = LM_CONFIG_USB_CONFIG_DESCRIPTOR_MAX_SIZE;

            struct device_descriptor_t
            {
                u16 vendor_id  = 0x303A; // The primary USB Vendor ID (VID) for Espressif Systems.
                u16 product_id = 0x80C4; // Lolin S2 Mini - UF2 Bootloader
                u16 bcd_device = 0x0100; // Device Release Number (1.0)
            } device_descriptor = {};

            struct string_descriptors_t
            {
                char manufacturer[32] = "Cirfrey Inc.";
                char product[32]      = "loom.a.ne Multidevice";
                char midi[32]         = "loom.a.ne MIDI";
                char hid[32]          = "loom.a.ne HID";
                char uac[32]          = "loom.a.ne UAC";
                char cdc[32]          = "loom.a.ne CDC";
                char msc[32]          = "loom.a.ne MSC";
            } string_descriptors = {};
        } usb = {};

        struct usbip_t
        {
            u16 port = 3240;
            bool close_conn_after_devlist = true;

            #ifndef LM_CONFIG_USBIP_MAX_ENDPOINTS
            #define LM_CONFIG_USBIP_MAX_ENDPOINTS 7
            #endif
            static constexpr u8 max_endpoints = LM_CONFIG_USBIP_MAX_ENDPOINTS;

            #ifndef LM_CONFIG_USBIP_CONFIG_DESCRIPTOR_MAX_SIZE
            #define LM_CONFIG_USBIP_CONFIG_DESCRIPTOR_MAX_SIZE (128 * 6)
            #endif
            // The descriptor can get very large when you enable multiple things.
            // This should be able to handle UAC + HID + MIDI (16 cables)
            static constexpr u16 config_descriptor_max_size = LM_CONFIG_USBIP_CONFIG_DESCRIPTOR_MAX_SIZE;

            struct device_descriptor_t
            {
                u16 vendor_id  = 0x303A; // The primary USB Vendor ID (VID) for Espressif Systems.
                u16 product_id = 0x80C4; // Lolin S2 Mini - UF2 Bootloader
                u16 bcd_device = 0x0100; // Device Release Number (1.0)
            } device_descriptor = {};

            struct string_descriptors_t
            {
                char manufacturer[32] = "Cirfrey Inc.";
                char product[32]      = "loom.a.ne Multidevice";
                char midi[32]         = "loom.a.ne MIDI";
                char hid[32]          = "loom.a.ne HID";
                char uac[32]          = "loom.a.ne UAC";
                char cdc[32]          = "loom.a.ne CDC";
                char msc[32]          = "loom.a.ne MSC";
            } string_descriptors = {};
        } usbip;

        struct cdc_t {
            struct backend_t {
                // Used for identifying the backend on events.
                enum class ids : u8{
                    usb, usbip, mesh,
                    count
                };

                struct usb_t {
                    feature toggle = feature::off;
                    feature strict = feature::off;
                } usb;
                struct usbip_t {
                    feature toggle = feature::off;
                } usbip;
                struct mesh_t {
                    feature toggle = feature::off;
                } mesh;
            } backend;
        } cdc;

        struct audio_t
        {
            struct backend_t {
                enum class ids : u8 {
                    usb, usbip, mesh,
                    count
                };
                struct usb_t {
                    // Enable just by setting the channels.
                    u8 speaker_channels    = 0;
                    u8 microphone_channels = 0;
                } usb;
                struct usbip_t {
                    u8 speaker_channels    = 0;
                    u8 microphone_channels = 0;
                } usbip;
                struct mesh_t {
                    // Don't need to setup channels, each message
                    // is prefixed with the direction (speaker/microphone - u1)
                    // and the channel id (u7 - up to 128 channels).
                    feature toggle = feature::off;
                } mesh;
            } backend;
        } audio;


        struct hid_t {
            struct backend_t {
                enum class ids : u8 {
                    usb, usbip, mesh
                };
                struct usb_t {
                    feature toggle = feature::off;
                    u8      pooling_interval_ms = 5;
                } usb;
                struct usbip_t {
                    feature toggle = feature::off;
                } usbip;
                struct mesh_t {
                    feature toggle = feature::off;
                } mesh;
            } backend;
        } hid;

        // TODO: refactor me.
        enum midi_t : u8 {
            midi_off,    // 0 EP.
            midi_in,     // 1 EP.
            midi_out,    // 1 EP.
            midi_inout,  // 2 EP.
            midi_inout_strict,
        } midi = midi_off;
        u8 midi_cable_count = 1;
        static const u8 midi_max_cable_count = 16;

        // TODO: refactor me.
        enum msc_t : u8 {
            msc_off,
            msc_on,
            msc_on_strict,
        } msc = msc_off;
    };

    extern config_t config;
}

// TODO: refactor me!
namespace lm::_config::strand
{
    constexpr auto apply_config = fabric::strand_constants{
        .name = "lm::apply_config",
        .priority = 2,
        .stack_size = 64 * 128,
        .core_affinity = 1,
    };

    // Most timing-critical strand.
    // Must service 1ms SOF interrupts, If this lags, USB disconnects.
    constexpr auto usbd = fabric::strand_constants{
        .name = "lm::usbd",
        .priority = 2,
        .stack_size = 64 * 128,
        .core_affinity = 1,
    };

    constexpr auto usbip = fabric::strand_constants{
        .name = "lm::usbip",
        .priority = 5,
        .stack_size = 64 * 128,
        .core_affinity = 1,
    };

    constexpr auto tud = fabric::strand_constants{
        .name = "tinyusb(tud)",
        .priority = 6,
        .stack_size = 64 * 128,
        .core_affinity = 1,
    };

    // ESPNOW / Audio Pump.
    constexpr auto radio = fabric::strand_constants{
        .name = "lm::radio",
        .priority = 5,
        .stack_size = 24 * 128,
        .core_affinity = 0,
    };

    constexpr auto strandman = fabric::strand_constants{
        .name = "lm::strandman",
        .priority = 4,
        .stack_size = 26 * 128,
        .core_affinity = 1,
    };

    // Captive portal, MIDI mapping, debouncing, etc.
    constexpr auto app = fabric::strand_constants{
        .name = "lm::app",
        .priority = 3,
        .stack_size = 32 * 128,
        .core_affinity = 1,
    };

    // The strand that handles flushing the log buffers to their
    // respective sinks (CDC, HID Vendor, ESPNOW).
    constexpr auto logging = fabric::strand_constants{
        .name = "lm::logging",
        .priority = 2,
        .stack_size = 24 * 128,
        .core_affinity = 1,
    };

    // General health monitor (RAM, Memory usage, Core usage, etc).
    constexpr auto healthmon = fabric::strand_constants{
        .name = "lm::healthmon",
        .priority = 1,
        .stack_size = 22 * 128,
        .core_affinity = 1,
    };

    // Monitors and prints messages posted on the bus.
    constexpr auto busmon = fabric::strand_constants{
        .name = "lm::busmon",
        .priority = 1,
        .stack_size = 18 * 128,
        .core_affinity = 1,
    };

    constexpr auto blink = fabric::strand_constants{
        .name = "lm::blink",
        .priority = 1,
        .stack_size = 11 * 128,
        .core_affinity = 1,
    };

    // Just so we have something to put in the array below.
    constexpr auto dummy = fabric::strand_constants{
        .name = "lm::dummy",
        .priority = 0,
        .stack_size = 0,
        .core_affinity = fabric::strand_constants::no_affinity,
    };
}

// TODO: refactor me!
namespace lm::_config::storage
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
