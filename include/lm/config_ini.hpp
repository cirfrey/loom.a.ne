// Defines the ini mapping for the lm::config struct.
#pragma once

#include "lm/config.hpp"
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
    constexpr auto many(char const* key, Enum& val) -> ini::field
    { return ini::enumeration_field(key | to_text, val); }
    constexpr auto feat(char const* key, feature& f) -> ini::field
    { return ini::normalized_enumeration_field<feature_ini, feature, feature_normalizer, 0, 5>(key | to_text, f); }
    template <typename Num>
    constexpr auto numb(char const* key, Num& num, ini::field::number_data_t data = {}) -> ini::field
    { return ini::number_field(key | to_text, num, data); }
    constexpr auto strg(char const* key, char* str, ini::field::string_data_t data) -> ini::field
    { return ini::string_field(key | to_text, str, data); }

    // All the fields we want to expose to ini configuration.
    // This is usually read in lm::hooks::arch_config().
    // If you want to have your own fields, define them and read the ini with them in your lm::hooks::config(),
    // just remember to set log_ignored = false in the parse args so you don't flood yourself with unset fields.
    inline ini::field fields[] = {
        /// --- Network ---
        strg("network.ssid", config.network.ssid, {.max_len = config.network.ssid_max_len}),
        strg("network.password", config.network.password, {.max_len = config.network.password_max_len}),

        /// --- Framework ---
        numb("framework.manager_announce_window_ms", config.framework.manager_announce_window_ms),

        /// --- Logging ---
        feat("logging.debug.toggle",     config.logging.level_enabled[config_t::logging_t::level::debug]),
        feat("logging.test.toggle",      config.logging.level_enabled[config_t::logging_t::level::test]),
        feat("logging.assertion.toggle", config.logging.level_enabled[config_t::logging_t::level::assertion]),
        feat("logging.info.toggle",      config.logging.level_enabled[config_t::logging_t::level::info]),
        feat("logging.regular.toggle",   config.logging.level_enabled[config_t::logging_t::level::regular]),
        feat("logging.warn.toggle",      config.logging.level_enabled[config_t::logging_t::level::warn]),
        feat("logging.error.toggle",     config.logging.level_enabled[config_t::logging_t::level::error]),
        feat("logging.panic.toggle",     config.logging.level_enabled[config_t::logging_t::level::panic]),
        strg("logging.debug.ansi",       config.logging.level_ansi.data[config_t::logging_t::level::debug],       {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::debug]}),
        strg("logging.test.ansi",        config.logging.level_ansi.data[config_t::logging_t::level::test],        {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::test]}),
        strg("logging.assertion.ansi",   config.logging.level_ansi.data[config_t::logging_t::level::assertion],   {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::assertion]}),
        strg("logging.info.ansi",        config.logging.level_ansi.data[config_t::logging_t::level::info],        {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::info]}),
        strg("logging.regular.ansi",     config.logging.level_ansi.data[config_t::logging_t::level::regular],     {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::regular]}),
        strg("logging.warn.ansi",        config.logging.level_ansi.data[config_t::logging_t::level::warn],        {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::warn]}),
        strg("logging.error.ansi",       config.logging.level_ansi.data[config_t::logging_t::level::error],       {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::error]}),
        strg("logging.panic.ansi",       config.logging.level_ansi.data[config_t::logging_t::level::panic],       {.max_len = config_t::logging_t::ansi_max_len,   .size_out = &config.logging.level_ansi.size[config_t::logging_t::level::panic]}),
        strg("logging.test.prefix",      config.logging.level_prefix.data[config_t::logging_t::level::test],      {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::test]}),
        strg("logging.assertion.prefix", config.logging.level_prefix.data[config_t::logging_t::level::assertion], {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::assertion]}),
        strg("logging.info.prefix",      config.logging.level_prefix.data[config_t::logging_t::level::info],      {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::info]}),
        strg("logging.regular.prefix",   config.logging.level_prefix.data[config_t::logging_t::level::regular],   {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::regular]}),
        strg("logging.warn.prefix",      config.logging.level_prefix.data[config_t::logging_t::level::warn],      {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::warn]}),
        strg("logging.error.prefix",     config.logging.level_prefix.data[config_t::logging_t::level::error],     {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::error]}),
        strg("logging.panic.prefix",     config.logging.level_prefix.data[config_t::logging_t::level::panic],     {.max_len = config_t::logging_t::prefix_max_len, .size_out = &config.logging.level_prefix.size[config_t::logging_t::level::panic]}),
        many("logging.timestamp", config.logging.timestamp),
        many("logging.filename", config.logging.filename),
        feat("logging.color", config.logging.color),
        feat("logging.prefix", config.logging.prefix),
        feat("logging.toggle", config.logging.toggle),

        /// --- Test ---
        feat("test.unit", config.test.unit),
        many("test.after_unit", config.test.after_unit),

        /// TODO: --- Launcher ---
        /// TODO: --- Strandman ---

        /// --- Usb ---
        numb("usb.device.class",    config.usb.device_descriptor.device_class),
        numb("usb.device.subclass", config.usb.device_descriptor.device_subclass),
        numb("usb.device.protocol", config.usb.device_descriptor.device_protocol),
        numb("usb.device.vendor",   config.usb.device_descriptor.vendor_id),
        numb("usb.device.product",  config.usb.device_descriptor.product_id),
        numb("usb.device.bcd",      config.usb.device_descriptor.bcd_device),
        strg("usb.string.manufacturer", config.usb.string_descriptors.manufacturer, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.product",      config.usb.string_descriptors.product,      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.serial",       config.usb.string_descriptors.serial,       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.midi",         config.usb.string_descriptors.midi,         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.hid",          config.usb.string_descriptors.hid,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.uac",          config.usb.string_descriptors.uac,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.cdc",          config.usb.string_descriptors.cdc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usb.string.msc",          config.usb.string_descriptors.msc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),

        /// --- Usbip ---
        numb("usbip.port", config.usbip.port),
        feat("usbip.close_conn_after_devlist", config.usbip.close_conn_after_devlist),

        numb("usbip.stall_status_code", config.usbip.stall_status_code),

        strg("usbip.path", config.usbip.path,  {.max_len = sizeof(config_t::usbip_t::path)}),
        strg("usbip.busid", config.usbip.busid, {.max_len = sizeof(config_t::usbip_t::busid)}),

        numb("usbip.out_event_queue_size", config.usbip.out_event_queue_size),

        numb("usbip.device.class",    config.usbip.device_descriptor.device_class),
        numb("usbip.device.subclass", config.usbip.device_descriptor.device_subclass),
        numb("usbip.device.protocol", config.usbip.device_descriptor.device_protocol),
        numb("usbip.device.vendor",   config.usbip.device_descriptor.vendor_id),
        numb("usbip.device.product",  config.usbip.device_descriptor.product_id),
        numb("usbip.device.bcd",      config.usbip.device_descriptor.bcd_device),
        strg("usbip.string.manufacturer", config.usbip.string_descriptors.manufacturer, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.product",      config.usbip.string_descriptors.product,      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.serial",       config.usbip.string_descriptors.serial,       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.midi",         config.usbip.string_descriptors.midi,         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.hid",          config.usbip.string_descriptors.hid,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.uac",          config.usbip.string_descriptors.uac,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.cdc",          config.usbip.string_descriptors.cdc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        strg("usbip.string.msc",          config.usbip.string_descriptors.msc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),

        /// --- Audio ---
        // Capped to 2 channels for now.
        numb("audio.usb.microphone_channels",   config.audio.backend.usb.microphone_channels,   {.max = config.audio.backend.usb.max_channels}),
        numb("audio.usb.speaker_channels",      config.audio.backend.usb.speaker_channels,      {.max = config.audio.backend.usb.max_channels}),
        numb("audio.usbip.microphone_channels", config.audio.backend.usbip.microphone_channels, {.max = config.audio.backend.usbip.max_channels}),
        numb("audio.usbip.speaker_channels",    config.audio.backend.usbip.speaker_channels,    {.max = config.audio.backend.usbip.max_channels}),
        feat("audio.mesh.toggle",               config.audio.backend.mesh.toggle),

        /// --- CDC ---
        feat("cdc.usb.toggle",       config.cdc.backend.usb.toggle),
        feat("cdc.usb.strict_eps",   config.cdc.backend.usb.strict_eps),
        feat("cdc.usbip.toggle",     config.cdc.backend.usbip.toggle),
        feat("cdc.usbip.strict_eps", config.cdc.backend.usbip.strict_eps),
        feat("cdc.mesh.toggle",      config.cdc.backend.mesh.toggle),

        /// --- HID ---
        feat("hid.usb.toggle",    config.hid.backend.usb.toggle),
        numb("hid.usb.pool_ms",   config.hid.backend.usb.pool_ms),
        feat("hid.usbip.toggle",  config.hid.backend.usbip.toggle),
        numb("hid.usbip.pool_ms", config.hid.backend.usbip.pool_ms),
        feat("hid.mesh.toggle",   config.hid.backend.mesh.toggle),

        /// --- MIDI ---
        numb("midi.usb.cable_count",   config.midi.backend.usb.cable_count,   {.max = config_t::midi_t::max_cables}),
        many("midi.usb.mode",          config.midi.backend.usb.mode),
        feat("midi.usb.strict_eps",    config.midi.backend.usb.strict_eps),
        numb("midi.usbip.cable_count", config.midi.backend.usbip.cable_count, {.max = config_t::midi_t::max_cables}),
        many("midi.usbip.mode",        config.midi.backend.usbip.mode),
        feat("midi.usbip.strict_eps",  config.midi.backend.usbip.strict_eps),

        /// --- MSC ---
        feat("msc.usb.toggle",       config.msc.backend.usb.toggle),
        feat("msc.usb.strict_eps",   config.msc.backend.usb.strict_eps),
        feat("msc.usbip.toggle",     config.msc.backend.usbip.toggle),
        feat("msc.usbip.strict_eps", config.msc.backend.usbip.strict_eps),
    };
}
