#pragma once

#include "lm/core/types.hpp"
#include "lm/core/ansi.hpp"
#include "lm/fabric/types.hpp"
#include "lm/usb/common.hpp"

#include "lm/chip/memory.hpp"

// TODO: #include "loomane_config.h"
//       user overrides.

#include <span>

namespace lm
{
    // For config stuff, use this instead of bools (it'll make the intent in code more clear and
    // interop with ini parsing via feature_ini.
    enum class feature : u8
    {
        disabled = 0, off = 0, no  = 0,
        enabled = 1,  on = 1,  yes = 1,
    };

    // The config object.
    // It has:
    // - Static fields (memory allocation stuff, or things that must be known
    //   at compile time) customizable by macros and;
    // - Dynamic fields (runtime stuff) customizable by regular assignment.
    struct config_t
    {
        // To get around some C++ limitations.
        template <st Rows, st Cols>
        struct table {
            static constexpr st rows = Rows;
            static constexpr st cols = Cols;

            char data[rows][cols]{};
            st   size[rows]{};

            constexpr auto copy(auto dst_idx, const auto& src) -> void
            {
                // TODO: Some asserts of size and handling '\0' would be nice here.
                size[dst_idx] = src.size;
                for(st i = 0; i < src.size; ++i) data[dst_idx][i] = src.data[i];
            };
        };

        struct network_t
        {
            #ifndef LM_CONFIG_NETWORK_SSID_MAX_LEN
            #define LM_CONFIG_NETWORK_SSID_MAX_LEN 64
            #endif
            static constexpr st ssid_max_len = LM_CONFIG_NETWORK_SSID_MAX_LEN;
            char ssid[ssid_max_len] = "loom.a.ne";

            #ifndef LM_CONFIG_NETWORK_PASSWORD_MAX_LEN
            #define LM_CONFIG_NETWORK_PASSWORD_MAX_LEN 64
            #endif
            static constexpr st password_max_len = LM_CONFIG_NETWORK_PASSWORD_MAX_LEN;
            char password[password_max_len] = "loom.a.ne";
        } network;

        struct framework_t
        {
            // Refers to the manager_announce event.
            // This is the window you should expect responses from.
            u16 manager_announce_window_ms = 10;
        } framework;

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
                test, // Used for unit and manufacturing tests.
                assertion,
                info,
                regular,
                warn,
                error,
                panic,
                #ifdef LM_CONFIG_LOGGING_EXTRA_LEVELS
                    LM_CONFIG_LOGGING_EXTRA_LEVELS
                #endif

                level_count
            };

            feature level_enabled[level_count] = {
                [debug]     = feature::on,
                [test]      = feature::on,
                [assertion] = feature::on,
                [info]      = feature::on,
                [regular]   = feature::on,
                [warn]      = feature::on,
                [error]     = feature::on,
                [panic]     = feature::on, // NOTE: If you disable panic OR logging in general, then the program won't call
                                           // chip::system::panic when you do log::panic(). Make sure to call it yourself then.
                #ifdef LM_CONFIG_LOGGING_EXTRA_LEVELS_ENABLED
                    LM_CONFIG_LOGGING_EXTRA_LEVELS_ENABLED
                #endif
            };

            #ifndef LM_CONFIG_LOGGING_ANSI_MAX_LEN
            // Should be enough even for some truecolor shenanigans.
            #define LM_CONFIG_LOGGING_ANSI_MAX_LEN 32
            #endif
            static constexpr u8 ansi_max_len = LM_CONFIG_LOGGING_ANSI_MAX_LEN;
            table<level_count, ansi_max_len> level_ansi = []() consteval {
                table<level_count, ansi_max_len> out{};

                out.copy(debug,     ansi::code<ansi::fg::gray>);
                out.copy(test,      ansi::code<ansi::fg::blue>);
                out.copy(assertion, ansi::code<ansi::fg::bright_magenta, ansi::style::bold>);
                out.copy(info,      ansi::code<ansi::fg::white>);
                out.copy(regular,   ansi::code<ansi::style::reset>);
                out.copy(warn,      ansi::code<ansi::fg::yellow>);
                out.copy(error,     ansi::code<ansi::fg::red>);
                out.copy(panic,     ansi::code<ansi::fg::bright_red, ansi::style::bold>);

                #ifdef LM_CONFIG_LOGGING_EXTRA_LEVELS_ANSI
                    LM_CONFIG_LOGGING_EXTRA_LEVELS_ANSI
                #endif

                return out;
            }();

            #ifndef LM_CONFIG_LOGGING_PREFIX_MAX_LEN
            #define LM_CONFIG_LOGGING_PREFIX_MAX_LEN 8
            #endif
            static constexpr u8 prefix_max_len = LM_CONFIG_LOGGING_PREFIX_MAX_LEN;
            table<level_count, prefix_max_len> level_prefix = []() consteval {
                table<level_count, prefix_max_len> out{};

                out.copy(debug,     "[d]"_text);
                out.copy(test,      "[t]"_text);
                out.copy(assertion, "[a]"_text);
                out.copy(info,      "[i]"_text);
                out.copy(regular,   "[r]"_text);
                out.copy(warn,      "[W]"_text);
                out.copy(error,     "[E]"_text);
                out.copy(panic,     "[PANIC]"_text);

                #ifdef LM_CONFIG_LOGGING_EXTRA_LEVELS_PREFIX
                    LM_CONFIG_LOGGING_EXTRA_LEVELS_PREFIX
                #endif

                return out;
            }();

            enum timestamp_t {
                no_timestamp,
                timestamp_ms_6,
            } timestamp = timestamp_ms_6;
            enum filename_t {
                no_filename,
                short_filename,
                full_filename,
            } filename = short_filename;
            feature color = feature::on;
            feature prefix = feature::on;

            // TODO: configurable dispatching stream for each log level.

            // Also able disable logging in general.
            feature toggle = feature::on;
            // Or override all dispatchers with your own function.
            using dispatcher_t = bool(*)(text, level);
            dispatcher_t custom_dispatcher = nullptr;
        } logging;

        struct bus_t
        {
            #ifndef LM_CONFIG_BUS_MAX_SUBSCRIBERS
            #define LM_CONFIG_BUS_MAX_SUBSCRIBERS 32
            #endif
            static constexpr u16 max_subscribers = LM_CONFIG_BUS_MAX_SUBSCRIBERS;
        } bus;

        struct test_t
        {
            feature unit = feature::on;
            enum after_test
            {
                proceed,
                proceed_if_ok,
                halt,
            };
            after_test after_unit = proceed;
        } test;

        // These are only valid if you are using the launcher.
        struct launcher_t
        {
            #ifndef LM_CONFIG_LAUNCHER_STRANDMAN_MAX_STRANDS
            #define LM_CONFIG_LAUNCHER_STRANDMAN_MAX_STRANDS 16
            #endif
            static constexpr u16 strandman_max_strands = LM_CONFIG_LAUNCHER_STRANDMAN_MAX_STRANDS;

            struct info {
                char name[24];
                u16 stack_size;
                feature set_running;

                [[deprecated("No longer used, removeme!")]] st priority;
                [[deprecated("No longer used, will this be necessary?")]] st core_affinity;

                u16 sleep_ms = 0;
            };

            info strandman = info{
                .name        = "lm.strandman",
                .stack_size  = 26 * 128,
                .set_running = feature::on,
            };

            info log = info{
                .name        = "lm.log",
                .stack_size  = 24 * 128,
                .set_running = feature::on,
            };

            info apply_config = info{
                .name        = "lm.config",
                .stack_size  = 64 * 128,
                .set_running = feature::on,
            };

            info healthmon = info{
                .name        = "lm::strand::healthmon",
                .stack_size  = 22 * 128,
                .set_running = feature::on,
            };

            info blink = info{
                .name        = "lm::strand::blink",
                .stack_size  = 11 * 128,
                .set_running = feature::on,
            };

            info busmon = info{
                .name        = "lm::strand::busmon",
                .stack_size  = 18 * 128,
                .set_running = feature::on,
            };

            info usbd = info{
                .name        = "lm::strand::usbd",
                .stack_size  = 64 * 128,
                .set_running = feature::on,
            };

            // Internal to usbd, not managed.
            info tud = info{
                .name        = "lm::strand::tud",
                .stack_size  = 64 * 128,
                .set_running = feature::on,
            };

            info usbip = info{
                .name        = "lm::strand::usbip",
                .stack_size  = 64 * 128,
                .set_running = feature::on,
            };

        } launcher;

        struct strandman_t
        {
            #ifndef LM_CONFIG_STRANDMAN_MAX_DEPENDS
            #define LM_CONFIG_STRANDMAN_MAX_DEPENDS 4
            #endif
            static constexpr u8 max_depends = LM_CONFIG_STRANDMAN_MAX_DEPENDS;

            u8 status_queue_size    = 16;
            u8 strandman_queue_size = 64;
        } strandman;

        // Common between usb_t and usbip_t.
        struct usbcommon
        {
            struct device_descriptor
            {
                u8 device_class    = 0xEF;  // TUSB_CLASS_MISC.
                u8 device_subclass = 2;     // MISC_CLASS_COMMON.
                u8 device_protocol = 1;     // MISC_PROTOCOL_UAD.
                u16 vendor_id  = 0x0000;
                u16 product_id = 0x0000;
                u16 bcd_device = 0x0000;
            };

            #ifndef LM_CONFIG_USBCOMMON_STRING_DESCRIPTOR_MAX_LEN
            #define LM_CONFIG_USBCOMMON_STRING_DESCRIPTOR_MAX_LEN 32
            #endif
            static constexpr u8 string_descriptor_max_len = LM_CONFIG_USBCOMMON_STRING_DESCRIPTOR_MAX_LEN;

            using string_descriptor = char[string_descriptor_max_len];
            struct string_descriptors
            {
                string_descriptor manufacturer = "Cirfrey Inc.";
                string_descriptor product      = "loom.a.ne";
                // Usually overriden in lm::hook::arch_config(), since the ini parsing and lm::hook::config()
                // run after that, it means you can actually override what the arch_config puts here.
                // If for whatever reason you were so inclined...
                string_descriptor serial       = "00:00:00:00:00:00";
                string_descriptor midi         = "loom.a.ne MIDI";
                string_descriptor hid          = "loom.a.ne HID";
                string_descriptor uac          = "loom.a.ne UAC";
                string_descriptor cdc          = "loom.a.ne CDC";
                string_descriptor msc          = "loom.a.ne MSC";
            };
        };

        struct usb_t
        {
            // Generally set in lm::hook::arch_config.
            std::span<usb::ep_t> endpoints;

            #ifndef LM_CONFIG_USB_CONFIG_DESCRIPTOR_MAX_SIZE
            // The descriptor can get very large when you enable multiple things.
            // This should be able to handle UAC + HID + MIDI (16 cables)
            #define LM_CONFIG_USB_CONFIG_DESCRIPTOR_MAX_SIZE (128 * 6)
            #endif
            static constexpr u16 config_descriptor_max_size = LM_CONFIG_USB_CONFIG_DESCRIPTOR_MAX_SIZE;

            usbcommon::device_descriptor  device_descriptor;
            usbcommon::string_descriptors string_descriptors;
        } usb = {};

        struct usbip_t
        {
            u16 port = 3240;
            feature close_conn_after_devlist = feature::on;

            // You most likely will never want to touch this.
            // Its for the gross nasty internals of USB/IP.
            u32 stall_status_code = 32; /*EPIPE*/

            char path[64]  = "/sys/bus/usb/devices/1-1";
            char busid[16] = "1-1";

            // How many events it can take before it starts dropping.
            // Remember this is includes extensions and all output events.
            u16 out_event_queue_size = 256;

            #ifndef LM_CONFIG_USBIP_MAX_ENDPOINTS
            // Since this is virtual and doens't connect to real hardware, we
            // can have as many endpoints as we need. Just know each endpoint
            // takes some memory. 8 is more than enough for all features.
            #define LM_CONFIG_USBIP_MAX_ENDPOINTS 8
            #endif
            static constexpr u8 max_endpoints = LM_CONFIG_USBIP_MAX_ENDPOINTS;
            // Generally set in lm::hook::arch_config.
            std::span<usb::ep_t> endpoints;


            #ifndef LM_CONFIG_USBIP_CONFIG_DESCRIPTOR_MAX_SIZE
            // The descriptor can get very large when you enable multiple things.
            // This should be able to handle UAC + HID + MIDI (16 cables)
            #define LM_CONFIG_USBIP_CONFIG_DESCRIPTOR_MAX_SIZE (128 * 6)
            #endif
            static constexpr u16 config_descriptor_max_size = LM_CONFIG_USBIP_CONFIG_DESCRIPTOR_MAX_SIZE;

            usbcommon::device_descriptor  device_descriptor;
            usbcommon::string_descriptors string_descriptors;
        } usbip;

        struct audio_t
        {
            struct backend_t {
                struct id { enum id_t : u8 {
                    usb, usbip, mesh,
                    count
                }; };
                struct usb_t {
                    static constexpr auto max_channels = 2;
                    // Enable just by setting the channels.
                    u8 microphone_channels = 0;
                    u8 speaker_channels    = 0;
                } usb;
                struct usbip_t {
                    static constexpr auto max_channels = 2;
                    u8 microphone_channels = 0;
                    u8 speaker_channels    = 0;
                } usbip;
                struct mesh_t {
                    // Don't need to setup channels, each message
                    // is prefixed with the direction (speaker/microphone - u1)
                    // and the channel id (u7 - up to 128 channels).
                    feature toggle = feature::off;
                } mesh;
            } backend;
        } audio;

        struct cdc_t {
            struct backend_t {
                // Used for identifying the backend on events.
                struct id { enum id_t : u8{
                    usb, usbip, mesh,
                    count
                }; };
                struct usb_t {
                    feature toggle = feature::off;
                    feature strict_eps = feature::off;
                } usb;
                struct usbip_t {
                    feature toggle = feature::off;
                    feature strict_eps = feature::off;
                } usbip;
                struct mesh_t {
                    feature toggle = feature::off;
                } mesh;
            } backend;
        } cdc;

        struct hid_t {
            struct backend_t {
                struct id { enum id_t : u8 {
                    usb, usbip, mesh,
                    count
                }; };
                struct usb_t {
                    feature toggle = feature::off;
                    u8      pool_ms = 5;
                } usb;
                struct usbip_t {
                    feature toggle = feature::off;
                    u8      pool_ms = 5;
                } usbip;
                struct mesh_t {
                    feature toggle = feature::off;
                } mesh;
            } backend;
        } hid;

        struct midi_t
        {
            enum mode_t
            {
                in,
                out,
                inout
            };

            static const u8 max_cables = 16;

            struct backend_t {
                struct id { enum id_t : u8 {
                    usb, usbip, mesh, serial, rtp_midi, ble_midi,
                    count
                }; };
                struct usb_t {
                    u8 cable_count     = 0;
                    mode_t mode        = mode_t::inout;
                    feature strict_eps = feature::off;
                } usb;
                struct usbip_t {
                    u8 cable_count     = 0;
                    mode_t mode        = mode_t::inout;
                    feature strict_eps = feature::off;
                } usbip;
            } backend;
        } midi;

        struct msc_t
        {
            // NOTE: these must be space-padded.
            struct partition_desc_t
            {
                u8 vendor_id[8]   = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
                u8 product_id[16] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
                                     ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
                u8 product_rev[4] = {' ', ' ', ' ', ' '};
            };

            // If storage == nullptr this is considered unused.
            struct partition_t
            {
                u8 lun                            = 0;
                partition_desc_t       descriptor = {};
                chip::memory::storage* storage    = nullptr;
            };

            // NOTE: these are the maximum amount of partitions EXPOSED to msc, not
            // the maximum partitions on the device itself.
            #ifndef LM_CONFIG_MSC_MAX_PARTITIONS
            #define LM_CONFIG_MSC_MAX_PARTITIONS 4
            #endif
            // Generally set by lm::hook::arch_config.
            partition_t partitions[LM_CONFIG_MSC_MAX_PARTITIONS] = {};

            struct backend_t {
                struct id { enum id_t : u8 {
                    usb, usbip,
                    count
                }; };
                struct usb_t {
                    feature toggle     = feature::off;
                    feature strict_eps = feature::off;
                } usb;
                struct usbip_t {
                    feature toggle     = feature::off;
                    feature strict_eps = feature::off;
                } usbip;
            } backend;
        } msc;
    };

    // TODO: thread-safe, acessible by config().
    extern config_t config;
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
