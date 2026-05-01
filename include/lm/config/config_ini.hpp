// Defines the ini mapping for the lm::config struct.
#pragma once

#include "lm/config.hpp"
#include "lm/config/config_rw.hpp"
#include "lm/ini.hpp"

namespace lm::config_ini
{
    // ── fields ────────────────────────────────────────────────────────────────
    //
    // All fields exposed to ini configuration.
    // Typically read in lm::hooks::arch_config().
    //
    // If you define your own field list, pass log_ignored = false in parse_args
    // so unrecognised keys from this list don't flood your logs.

    inline ini::field fields[] = {
        /// x86_64 fields, generally set in lm::hook::arch_config.
        ini::str("native.inipath"_text, (char*)nullptr),

        /// --- System ---
        ini::feat("system.use_seed"_text, config_rw.system.use_seed),
        ini::num("system.random_seed.%u"_text, config_t::system_t::random_seed_count, +[](u8 i) { return &config_rw.system.random_seed[i]; }),

        /// --- Network ---
        ini::str("network.ssid"_text,     config_rw.network.ssid,     {.max_len = config_rw.network.ssid_max_len}),
        ini::str("network.password"_text, config_rw.network.password, {.max_len = config_rw.network.password_max_len}),

        /// --- Framework ---
        ini::num("framework.manager_request_timeout_ms"_text, config_rw.framework.manager_request_timeout_ms),

        /// --- Logging ---
        ini::enm("logging.timestamp"_text, config_rw.logging.timestamp),
        ini::enm("logging.filename"_text,  config_rw.logging.filename),
        ini::feat("logging.color"_text,    config_rw.logging.color),
        ini::feat("logging.prefix"_text,   config_rw.logging.prefix),
        ini::feat("logging.toggle"_text,   config_rw.logging.toggle),
        ini::feat(
            {
                .pattern   = "logging.%.*s.toggle"_text,
                .count     = config_t::logging_t::level_count,
                .formatter = ini::multikey::renum_formatter<config_t::logging_t::level_t>
            },
            +[](u8 i) { return &config_rw.logging.level[i | toe].enabled; }
        ),
        ini::str(
            {
                .pattern   = "logging.%.*s.ansi"_text,
                .count     = config_t::logging_t::level_count,
                .formatter = ini::multikey::renum_formatter<config_t::logging_t::level_t>
            },
            +[](u8 i) { return config_rw.logging.level[i | toe].ansi_storage; },
            {
                .max_len = config_t::logging_t::ansi_max_len,
                .size_out = [](u8 i, st v){ config_rw.logging.level[i | toe].ansi_len = v; }
            }
        ),
        ini::str(
            {
                .pattern   = "logging.%.*s.prefix"_text,
                .count     = config_t::logging_t::level_count,
                .formatter = ini::multikey::renum_formatter<config_t::logging_t::level_t>
            },
            +[](u8 i) { return config_rw.logging.level[i | toe].prefix_storage; },
            {
                .max_len = config_t::logging_t::prefix_max_len,
                .size_out = [](u8 i, st v){ config_rw.logging.level[i | toe].prefix_len = v; }
            }
        ),

        /// --- Test ---
        ini::feat("test.unit"_text,      config_rw.test.unit),
        ini::enm("test.after_unit"_text, config_rw.test.after_unit),

        /// TODO: --- Launcher ---
        /// TODO: --- Strandman ---

        /// --- Usb ---
        ini::num("usb.device.class"_text,        config_rw.usb.device_descriptor.device_class),
        ini::num("usb.device.subclass"_text,     config_rw.usb.device_descriptor.device_subclass),
        ini::num("usb.device.protocol"_text,     config_rw.usb.device_descriptor.device_protocol),
        ini::num("usb.device.vendor"_text,       config_rw.usb.device_descriptor.vendor_id),
        ini::num("usb.device.product"_text,      config_rw.usb.device_descriptor.product_id),
        ini::num("usb.device.bcd"_text,          config_rw.usb.device_descriptor.bcd_device),
        ini::str("usb.string.manufacturer"_text, config_rw.usb.string_descriptors.manufacturer, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.product"_text,      config_rw.usb.string_descriptors.product,      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.serial"_text,       config_rw.usb.string_descriptors.serial,       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.midi"_text,         config_rw.usb.string_descriptors.midi,         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.hid"_text,          config_rw.usb.string_descriptors.hid,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.uac"_text,          config_rw.usb.string_descriptors.uac,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.cdc"_text,          config_rw.usb.string_descriptors.cdc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usb.string.msc"_text,          config_rw.usb.string_descriptors.msc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),

        /// --- Usbip ---
        #if LM_CONFIG_USBIP_COUNT >= 1
        ini::num("usbip.%u.port"_text,                      config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].port; }),
        ini::feat("usbip.%u.close_conn_after_devlist"_text, config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].close_conn_after_devlist; }),
        ini::num("usbip.%u.stall_status_code"_text,         config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].stall_status_code; }),
        ini::str("usbip.%u.path"_text,                      config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].path; },  {.max_len = sizeof(config_t::usbip_t::path)}),
        ini::str("usbip.%u.busid"_text,                     config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].busid; }, {.max_len = sizeof(config_t::usbip_t::busid)}),
        ini::num("usbip.%u.out_event_queue_size"_text,      config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].out_event_queue_size; }),
        ini::num("usbip.%u.device.class"_text,              config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].device_descriptor.device_class; }),
        ini::num("usbip.%u.device.subclass"_text,           config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].device_descriptor.device_subclass; }),
        ini::num("usbip.%u.device.protocol"_text,           config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].device_descriptor.device_protocol; }),
        ini::num("usbip.%u.device.vendor"_text,             config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].device_descriptor.vendor_id; }),
        ini::num("usbip.%u.device.product"_text,            config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].device_descriptor.product_id; }),
        ini::num("usbip.%u.device.bcd"_text,                config_t::usbip_count, +[](u8 i) { return &config_rw.usbip[i].device_descriptor.bcd_device; }),
        ini::str("usbip.%u.string.manufacturer"_text,       config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.manufacturer; }, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.product"_text,            config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.product; },      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.serial"_text,             config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.serial; },       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.midi"_text,               config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.midi; },         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.hid"_text,                config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.hid; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.uac"_text,                config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.uac; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.cdc"_text,                config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.cdc; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        ini::str("usbip.%u.string.msc"_text,                config_t::usbip_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.msc; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        #endif

        /// --- Controllers ---
        #if LM_CONFIG_CONTROLLER_COUNT >= 1
            ini::num("controller.%u.vendor_id"_text,    config_t::controller_count, +[](u8 i) { return &config_rw.controller[i].vendor_id; }),
            ini::num("controller.%u.product_id"_text,   config_t::controller_count, +[](u8 i) { return &config_rw.controller[i].product_id; }),
            ini::num("controller.%u.bcd_device"_text,   config_t::controller_count, +[](u8 i) { return &config_rw.controller[i].bcd_device; }),
            ini::str("controller.%u.manufacturer"_text, config_t::controller_count, +[](u8 i) { return config_rw.controller[i].manufacturer; }, {.max_len = config_t::controller_t::string_descriptor_max_len}),
            ini::str("controller.%u.product"_text,      config_t::controller_count, +[](u8 i) { return config_rw.controller[i].product; },      {.max_len = config_t::controller_t::string_descriptor_max_len}),
            ini::str("controller.%u.serial"_text,       config_t::controller_count, +[](u8 i) { return config_rw.controller[i].serial; },       {.max_len = config_t::controller_t::string_descriptor_max_len}),
            // ini::str("controller.%u.midi"_text,               config_t::controller_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.midi; },         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
            // ini::str("controller.%u.hid"_text,                config_t::controller_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.hid; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
            // ini::str("controller.%u.uac"_text,                config_t::controller_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.uac; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
            // ini::str("controller.%u.cdc"_text,                config_t::controller_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.cdc; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
            // ini::str("controller.%u.msc"_text,                config_t::controller_count, +[](u8 i) { return config_rw.usbip[i].string_descriptors.msc; },          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        #endif


        /// --- Audio ---
        ini::num("audio.usb.microphone_channels"_text,   config_rw.audio.backend.usb.microphone_channels,   {.max = config_rw.audio.backend.usb.max_channels}),
        ini::num("audio.usb.speaker_channels"_text,      config_rw.audio.backend.usb.speaker_channels,      {.max = config_rw.audio.backend.usb.max_channels}),
        ini::num("audio.usbip.microphone_channels"_text, config_rw.audio.backend.usbip.microphone_channels, {.max = config_rw.audio.backend.usbip.max_channels}),
        ini::num("audio.usbip.speaker_channels"_text,    config_rw.audio.backend.usbip.speaker_channels,    {.max = config_rw.audio.backend.usbip.max_channels}),
        ini::feat("audio.mesh.toggle"_text,              config_rw.audio.backend.mesh.toggle),

        /// --- CDC ---
        ini::feat("cdc.usb.toggle"_text,       config_rw.cdc.backend.usb.toggle),
        ini::feat("cdc.usb.strict_eps"_text,   config_rw.cdc.backend.usb.strict_eps),
        ini::feat("cdc.usbip.toggle"_text,     config_rw.cdc.backend.usbip.toggle),
        ini::feat("cdc.usbip.strict_eps"_text, config_rw.cdc.backend.usbip.strict_eps),
        ini::feat("cdc.mesh.toggle"_text,      config_rw.cdc.backend.mesh.toggle),

        /// --- HID ---
        ini::feat("hid.usb.toggle"_text,   config_rw.hid.backend.usb.toggle),
        ini::num("hid.usb.pool_ms"_text,   config_rw.hid.backend.usb.pool_ms),
        ini::feat("hid.usbip.toggle"_text, config_rw.hid.backend.usbip.toggle),
        ini::num("hid.usbip.pool_ms"_text, config_rw.hid.backend.usbip.pool_ms),
        ini::feat("hid.mesh.toggle"_text,  config_rw.hid.backend.mesh.toggle),

        /// --- MIDI ---
        ini::num("midi.usb.cable_count"_text,   config_rw.midi.backend.usb.cable_count,   {.max = config_t::midi_t::max_cables}),
        ini::enm("midi.usb.mode"_text,          config_rw.midi.backend.usb.mode),
        ini::feat("midi.usb.strict_eps"_text,   config_rw.midi.backend.usb.strict_eps),
        ini::num("midi.usbip.cable_count"_text, config_rw.midi.backend.usbip.cable_count, {.max = config_t::midi_t::max_cables}),
        ini::enm("midi.usbip.mode"_text,        config_rw.midi.backend.usbip.mode),
        ini::feat("midi.usbip.strict_eps"_text, config_rw.midi.backend.usbip.strict_eps),

        /// --- MSC ---
        ini::feat("msc.usb.toggle"_text,       config_rw.msc.backend.usb.toggle),
        ini::feat("msc.usb.strict_eps"_text,   config_rw.msc.backend.usb.strict_eps),
        ini::feat("msc.usbip.toggle"_text,     config_rw.msc.backend.usbip.toggle),
        ini::feat("msc.usbip.strict_eps"_text, config_rw.msc.backend.usbip.strict_eps),
    };
}
