// Defines the ini mapping for the lm::config struct.
#pragma once

#include "lm/config.hpp"
#include "lm/config/boot.hpp"
#include "lm/ini.hpp"

namespace lm::config_ini
{
    // The ini-facing version of lm::feature, use this for parsing and normalize into lm::feature.
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
    static constexpr auto to_feature(feature_ini f) -> feature
    {
        if(
            f == feature_ini::off ||
            f == feature_ini::disabled ||
            f == feature_ini::no
        ) return feature::off;
        return feature::on;
    }
    constexpr auto feature_normalizer(ini::field const& f) -> void
    {
        *(feature*)f.output = to_feature(*(feature_ini*)f.output);
    };

    // Some aliases.
    template <int Lo = -128, int Hi = 128, typename Enum>
    constexpr auto enumeration(char const* key, Enum& val) -> ini::field
    { return ini::enumeration_field(key | to_text, val); }
    constexpr auto feature(char const* key, lm::feature& f) -> ini::field
    { return ini::normalized_enumeration_field<feature_ini, lm::feature, feature_normalizer, 0, 5>(key | to_text, f); }
    template <typename Num>
    constexpr auto number(char const* key, Num& num, ini::field::number_data_t data = {}) -> ini::field
    { return ini::number_field(key | to_text, num, data); }
    constexpr auto string(char const* key, char* str, ini::field::string_data_t data) -> ini::field
    { return ini::string_field(key | to_text, str, data); }

    // All the fields we want to expose to ini configuration.
    // This is usually read in lm::hooks::arch_config().
    // If you want to have your own fields, define them and read the ini with them in your lm::hooks::config(),
    // just remember to set log_ignored = false in the parse args so you don't flood yourself with unset fields.
    inline ini::field fields[] = {
        /// --- System ---
        feature("system.use_random_seed", config_rw.system.use_random_seed),
        number("system.random_seed",      config_rw.system.random_seed[0]),
        number("system.random_seed.0",    config_rw.system.random_seed[0]),
        number("system.random_seed.1",    config_rw.system.random_seed[1]),
        number("system.random_seed.2",    config_rw.system.random_seed[2]),
        number("system.random_seed.3",    config_rw.system.random_seed[3]),
        number("system.random_seed.4",    config_rw.system.random_seed[4]),
        number("system.random_seed.5",    config_rw.system.random_seed[5]),
        number("system.random_seed.6",    config_rw.system.random_seed[6]),
        number("system.random_seed.7",    config_rw.system.random_seed[7]),


        /// --- Network ---
        string("network.ssid",     config_rw.network.ssid, {.max_len = config_rw.network.ssid_max_len}),
        string("network.password", config_rw.network.password, {.max_len = config_rw.network.password_max_len}),

        /// --- Framework ---
        number("framework.manager_announce_window_ms", config_rw.framework.manager_announce_window_ms),

        /// --- Logging ---
        feature("logging.debug.toggle",     config_rw.logging.level[config_t::logging_t::level_t::debug].enabled),
        feature("logging.test.toggle",      config_rw.logging.level[config_t::logging_t::level_t::test].enabled),
        feature("logging.assertion.toggle", config_rw.logging.level[config_t::logging_t::level_t::assertion].enabled),
        feature("logging.info.toggle",      config_rw.logging.level[config_t::logging_t::level_t::info].enabled),
        feature("logging.regular.toggle",   config_rw.logging.level[config_t::logging_t::level_t::regular].enabled),
        feature("logging.warn.toggle",      config_rw.logging.level[config_t::logging_t::level_t::warn].enabled),
        feature("logging.error.toggle",     config_rw.logging.level[config_t::logging_t::level_t::error].enabled),
        feature("logging.panic.toggle",     config_rw.logging.level[config_t::logging_t::level_t::panic].enabled),
        string("logging.debug.ansi",       config_rw.logging.level[config_t::logging_t::level_t::debug].ansi_storage,       {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::debug].ansi_len}),
        string("logging.test.ansi",        config_rw.logging.level[config_t::logging_t::level_t::test].ansi_storage,        {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::test].ansi_len}),
        string("logging.assertion.ansi",   config_rw.logging.level[config_t::logging_t::level_t::assertion].ansi_storage,   {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::assertion].ansi_len}),
        string("logging.info.ansi",        config_rw.logging.level[config_t::logging_t::level_t::info].ansi_storage,        {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::info].ansi_len}),
        string("logging.regular.ansi",     config_rw.logging.level[config_t::logging_t::level_t::regular].ansi_storage,     {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::regular].ansi_len}),
        string("logging.warn.ansi",        config_rw.logging.level[config_t::logging_t::level_t::warn].ansi_storage,        {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::warn].ansi_len}),
        string("logging.error.ansi",       config_rw.logging.level[config_t::logging_t::level_t::error].ansi_storage,       {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::error].ansi_len}),
        string("logging.panic.ansi",       config_rw.logging.level[config_t::logging_t::level_t::panic].ansi_storage,       {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config_rw.logging.level[config_t::logging_t::level_t::panic].ansi_len}),
        string("logging.debug.prefix",     config_rw.logging.level[config_t::logging_t::level_t::debug].prefix_storage,     {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::debug].prefix_len}),
        string("logging.test.prefix",      config_rw.logging.level[config_t::logging_t::level_t::test].prefix_storage,      {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::test].prefix_len}),
        string("logging.assertion.prefix", config_rw.logging.level[config_t::logging_t::level_t::assertion].prefix_storage, {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::assertion].prefix_len}),
        string("logging.info.prefix",      config_rw.logging.level[config_t::logging_t::level_t::info].prefix_storage,      {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::info].prefix_len}),
        string("logging.regular.prefix",   config_rw.logging.level[config_t::logging_t::level_t::regular].prefix_storage,   {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::regular].prefix_len}),
        string("logging.warn.prefix",      config_rw.logging.level[config_t::logging_t::level_t::warn].prefix_storage,      {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::warn].prefix_len}),
        string("logging.error.prefix",     config_rw.logging.level[config_t::logging_t::level_t::error].prefix_storage,     {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::error].prefix_len}),
        string("logging.panic.prefix",     config_rw.logging.level[config_t::logging_t::level_t::panic].prefix_storage,     {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config_rw.logging.level[config_t::logging_t::level_t::panic].prefix_len}),
        enumeration("logging.timestamp", config_rw.logging.timestamp),
        enumeration("logging.filename",  config_rw.logging.filename),
        feature("logging.color",  config_rw.logging.color),
        feature("logging.prefix", config_rw.logging.prefix),
        feature("logging.toggle", config_rw.logging.toggle),

        /// --- Test ---
        feature("test.unit", config_rw.test.unit),
        enumeration("test.after_unit", config_rw.test.after_unit),

        /// TODO: --- Launcher ---
        /// TODO: --- Strandman ---

        /// --- Usb ---
        number("usb.device.class",    config_rw.usb.device_descriptor.device_class),
        number("usb.device.subclass", config_rw.usb.device_descriptor.device_subclass),
        number("usb.device.protocol", config_rw.usb.device_descriptor.device_protocol),
        number("usb.device.vendor",   config_rw.usb.device_descriptor.vendor_id),
        number("usb.device.product",  config_rw.usb.device_descriptor.product_id),
        number("usb.device.bcd",      config_rw.usb.device_descriptor.bcd_device),
        string("usb.string.manufacturer", config_rw.usb.string_descriptors.manufacturer, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.product",      config_rw.usb.string_descriptors.product,      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.serial",       config_rw.usb.string_descriptors.serial,       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.midi",         config_rw.usb.string_descriptors.midi,         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.hid",          config_rw.usb.string_descriptors.hid,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.uac",          config_rw.usb.string_descriptors.uac,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.cdc",          config_rw.usb.string_descriptors.cdc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.msc",          config_rw.usb.string_descriptors.msc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),

        /// --- Usbip ---
        number("usbip.port", config_rw.usbip.port),
        feature("usbip.close_conn_after_devlist", config_rw.usbip.close_conn_after_devlist),

        number("usbip.stall_status_code", config_rw.usbip.stall_status_code),

        string("usbip.path", config_rw.usbip.path,  {.max_len = sizeof(config_t::usbip_t::path)}),
        string("usbip.busid", config_rw.usbip.busid, {.max_len = sizeof(config_t::usbip_t::busid)}),

        number("usbip.out_event_queue_size", config_rw.usbip.out_event_queue_size),

        number("usbip.device.class",    config_rw.usbip.device_descriptor.device_class),
        number("usbip.device.subclass", config_rw.usbip.device_descriptor.device_subclass),
        number("usbip.device.protocol", config_rw.usbip.device_descriptor.device_protocol),
        number("usbip.device.vendor",   config_rw.usbip.device_descriptor.vendor_id),
        number("usbip.device.product",  config_rw.usbip.device_descriptor.product_id),
        number("usbip.device.bcd",      config_rw.usbip.device_descriptor.bcd_device),
        string("usbip.string.manufacturer", config_rw.usbip.string_descriptors.manufacturer, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.product",      config_rw.usbip.string_descriptors.product,      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.serial",       config_rw.usbip.string_descriptors.serial,       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.midi",         config_rw.usbip.string_descriptors.midi,         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.hid",          config_rw.usbip.string_descriptors.hid,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.uac",          config_rw.usbip.string_descriptors.uac,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.cdc",          config_rw.usbip.string_descriptors.cdc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usbip.string.msc",          config_rw.usbip.string_descriptors.msc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),

        /// --- Audio ---
        number("audio.usb.microphone_channels",   config_rw.audio.backend.usb.microphone_channels,   {.max = config_rw.audio.backend.usb.max_channels}),
        number("audio.usb.speaker_channels",      config_rw.audio.backend.usb.speaker_channels,      {.max = config_rw.audio.backend.usb.max_channels}),
        number("audio.usbip.microphone_channels", config_rw.audio.backend.usbip.microphone_channels, {.max = config_rw.audio.backend.usbip.max_channels}),
        number("audio.usbip.speaker_channels",    config_rw.audio.backend.usbip.speaker_channels,    {.max = config_rw.audio.backend.usbip.max_channels}),
        feature("audio.mesh.toggle",              config_rw.audio.backend.mesh.toggle),

        /// --- CDC ---
        feature("cdc.usb.toggle",       config_rw.cdc.backend.usb.toggle),
        feature("cdc.usb.strict_eps",   config_rw.cdc.backend.usb.strict_eps),
        feature("cdc.usbip.toggle",     config_rw.cdc.backend.usbip.toggle),
        feature("cdc.usbip.strict_eps", config_rw.cdc.backend.usbip.strict_eps),
        feature("cdc.mesh.toggle",      config_rw.cdc.backend.mesh.toggle),

        /// --- HID ---
        feature("hid.usb.toggle",   config_rw.hid.backend.usb.toggle),
        number("hid.usb.pool_ms",   config_rw.hid.backend.usb.pool_ms),
        feature("hid.usbip.toggle", config_rw.hid.backend.usbip.toggle),
        number("hid.usbip.pool_ms", config_rw.hid.backend.usbip.pool_ms),
        feature("hid.mesh.toggle",  config_rw.hid.backend.mesh.toggle),

        /// --- MIDI ---
        number("midi.usb.cable_count",   config_rw.midi.backend.usb.cable_count,   {.max = config_t::midi_t::max_cables}),
        enumeration("midi.usb.mode",     config_rw.midi.backend.usb.mode),
        feature("midi.usb.strict_eps",   config_rw.midi.backend.usb.strict_eps),
        number("midi.usbip.cable_count", config_rw.midi.backend.usbip.cable_count, {.max = config_t::midi_t::max_cables}),
        enumeration("midi.usbip.mode",   config_rw.midi.backend.usbip.mode),
        feature("midi.usbip.strict_eps", config_rw.midi.backend.usbip.strict_eps),

        /// --- MSC ---
        feature("msc.usb.toggle",       config_rw.msc.backend.usb.toggle),
        feature("msc.usb.strict_eps",   config_rw.msc.backend.usb.strict_eps),
        feature("msc.usbip.toggle",     config_rw.msc.backend.usbip.toggle),
        feature("msc.usbip.strict_eps", config_rw.msc.backend.usbip.strict_eps),
    };
}
