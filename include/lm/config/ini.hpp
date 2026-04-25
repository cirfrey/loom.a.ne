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

    // NOTE: unary plus (+) forces a lambda to decay to function pointer.
    #define LM_INI_MULTIKEY( TO ) +[](u8 i){ return & TO; }
    #define LM_INI_MULTIKEY_STR( TO ) +[](u8 i){ return TO; }

    using formatter_t = ini::field::get_key_for_idx_t;
    constexpr formatter_t default_key_formatter = [](text key, mut_text buf, u8 key_idx) -> text
        { return {buf.data, (st)std::snprintf(buf.data, buf.size, key.data, key_idx)}; };
    template <typename Enum, int Lo = -128, int Hi = 128>
    constexpr formatter_t renum_formatter = [](text key, mut_text buf, u8 key_idx) -> text
    {
        auto re = renum<Enum, Lo, Hi>::unqualified((Enum)key_idx);
        return {buf.data, (st)std::snprintf(buf.data, buf.size, key.data, (int)re.size, re.data)};
    };

    template <typename Getter>
    constexpr auto multikey(
        char const* key_pattern,
        u8 size,
        Getter,
        formatter_t formatter = default_key_formatter
    ) -> ini::key_info_t
    {
        return {
            .key = key_pattern | to_text,
            .keycount = size,
            .key_formatter = formatter,
            .output_redirector = [](void* output, u8 key_idx) -> void*
                { return ((Getter)output)(key_idx); },
        };
    }

    template <int Lo = -128, int Hi = 128, typename Enum>
    constexpr auto enumeration(char const* key, Enum& val) -> ini::field
    { return ini::enumeration_field({.key = key | to_text}, val); }

    constexpr auto feature(char const* key, lm::feature& f) -> ini::field
    { return ini::normalized_enumeration_field<feature_ini, lm::feature, feature_normalizer, 0, 5>({.key = key | to_text}, f); }

    inline auto feature_multikey(
        char const* key_pattern,
        lm::feature*(*getter)(u8),
        u8 size,
        formatter_t formatter = default_key_formatter
    )
    {
        return ini::normalized_enumeration_field_raw< feature_ini, lm::feature, feature_normalizer, 0, 5 >(
            multikey(key_pattern, size, getter, formatter),
            (void*)getter
        );
    }

    template <typename Num>
    constexpr auto number(char const* key, Num& num, ini::field::number_data_t data = {}) -> ini::field
    { return ini::number_field({.key = key | to_text}, num, data); }

    template <typename Num>
    inline auto number_multikey(
        const char* key_pattern,
        Num*(*getter)(u8),
        u8 size,
        ini::field::number_data_t data = {},
        formatter_t formatter = default_key_formatter
    ) -> ini::field
    {
        return ini::number_field_raw<Num>(
            multikey(key_pattern, size, getter, formatter),
            (void*)getter,
            data
        );
    }

    constexpr auto string(char const* key, char* str, ini::field::string_data_t data) -> ini::field
    { return ini::string_field({.key = key | to_text}, str, data); }

    inline auto string_multikey(
        char const* key_pattern,
        char*(*getter)(u8),
        u8 size,
        ini::field::string_data_t data,
        formatter_t formatter = default_key_formatter
    ) -> ini::field
    {
        return ini::string_field_raw(
            multikey(key_pattern, size, getter, formatter),
            (void*)getter,
            data
        );
    }

    // All the fields we want to expose to ini configuration.
    // This is usually read in lm::hooks::arch_config().
    // If you want to have your own fields, define them and read the ini with them in your lm::hooks::config(),
    // just remember to set log_ignored = false in the parse args so you don't flood yourself with unset fields.
    inline ini::field fields[] = {
        /// x86_64 fields, generally set in lm::hook::arch_config.
        string("native.inipath", nullptr, {}),

        /// --- System ---
        feature("system.use_seed",               config_rw.system.use_seed),
        number_multikey("system.random_seed.%u", LM_INI_MULTIKEY(config_rw.system.random_seed[i]), config_t::system_t::random_seed_count),

        /// --- Network ---
        string("network.ssid",     config_rw.network.ssid, {.max_len = config_rw.network.ssid_max_len}),
        string("network.password", config_rw.network.password, {.max_len = config_rw.network.password_max_len}),

        /// --- Framework ---
        number("framework.manager_announce_window_ms", config_rw.framework.manager_announce_window_ms),

        /// --- Logging ---
        enumeration("logging.timestamp", config_rw.logging.timestamp),
        enumeration("logging.filename",  config_rw.logging.filename),
        feature("logging.color",         config_rw.logging.color),
        feature("logging.prefix",        config_rw.logging.prefix),
        feature("logging.toggle",        config_rw.logging.toggle),
        feature_multikey("logging.%.*s.toggle", LM_INI_MULTIKEY(config_rw.logging.level[i | toe].enabled),            config_t::logging_t::level_count,                                                                                                                                   renum_formatter<config_t::logging_t::level_t>),
        string_multikey("logging.%.*s.ansi",    LM_INI_MULTIKEY_STR(config_rw.logging.level[i | toe].ansi_storage),   config_t::logging_t::level_count, {.max_len = config_t::logging_t::ansi_max_len,   .size_out = [](u8 i, st v){ config_rw.logging.level[i | toe].ansi_len = v; }},   renum_formatter<config_t::logging_t::level_t>),
        string_multikey("logging.%.*s.prefix",  LM_INI_MULTIKEY_STR(config_rw.logging.level[i | toe].prefix_storage), config_t::logging_t::level_count, {.max_len = config_t::logging_t::prefix_max_len, .size_out = [](u8 i, st v){ config_rw.logging.level[i | toe].prefix_len = v; }}, renum_formatter<config_t::logging_t::level_t>),

        /// --- Test ---
        feature("test.unit", config_rw.test.unit),
        enumeration("test.after_unit", config_rw.test.after_unit),

        /// TODO: --- Launcher ---
        /// TODO: --- Strandman ---

        /// --- Usb ---
        number("usb.device.class",        config_rw.usb.device_descriptor.device_class),
        number("usb.device.subclass",     config_rw.usb.device_descriptor.device_subclass),
        number("usb.device.protocol",     config_rw.usb.device_descriptor.device_protocol),
        number("usb.device.vendor",       config_rw.usb.device_descriptor.vendor_id),
        number("usb.device.product",      config_rw.usb.device_descriptor.product_id),
        number("usb.device.bcd",          config_rw.usb.device_descriptor.bcd_device),
        string("usb.string.manufacturer", config_rw.usb.string_descriptors.manufacturer, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.product",      config_rw.usb.string_descriptors.product,      {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.serial",       config_rw.usb.string_descriptors.serial,       {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.midi",         config_rw.usb.string_descriptors.midi,         {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.hid",          config_rw.usb.string_descriptors.hid,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.uac",          config_rw.usb.string_descriptors.uac,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.cdc",          config_rw.usb.string_descriptors.cdc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string("usb.string.msc",          config_rw.usb.string_descriptors.msc,          {.max_len = config_t::usbcommon::string_descriptor_max_len}),

        /// --- Usbip ---
        number_multikey("usbip.%u.port",                      LM_INI_MULTIKEY(config_rw.usbip[i].port),                                config_t::usbip_t::instance_count),
        feature_multikey("usbip.%u.close_conn_after_devlist", LM_INI_MULTIKEY(config_rw.usbip[i].close_conn_after_devlist),            config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.stall_status_code",         LM_INI_MULTIKEY(config_rw.usbip[i].stall_status_code),                   config_t::usbip_t::instance_count),
        string_multikey("usbip.%u.path",                      LM_INI_MULTIKEY_STR(config_rw.usbip[i].path),                            config_t::usbip_t::instance_count, {.max_len = sizeof(config_t::usbip_instance_t::path)}),
        string_multikey("usbip.%u.busid",                     LM_INI_MULTIKEY_STR(config_rw.usbip[i].busid),                           config_t::usbip_t::instance_count, {.max_len = sizeof(config_t::usbip_instance_t::busid)}),
        number_multikey("usbip.%u.out_event_queue_size",      LM_INI_MULTIKEY(config_rw.usbip[i].out_event_queue_size),                config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.device.class",              LM_INI_MULTIKEY(config_rw.usbip[i].device_descriptor.device_class),      config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.device.subclass",           LM_INI_MULTIKEY(config_rw.usbip[i].device_descriptor.device_subclass),   config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.device.protocol",           LM_INI_MULTIKEY(config_rw.usbip[i].device_descriptor.device_protocol),   config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.device.vendor",             LM_INI_MULTIKEY(config_rw.usbip[i].device_descriptor.vendor_id),         config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.device.product",            LM_INI_MULTIKEY(config_rw.usbip[i].device_descriptor.product_id),        config_t::usbip_t::instance_count),
        number_multikey("usbip.%u.device.bcd",                LM_INI_MULTIKEY(config_rw.usbip[i].device_descriptor.bcd_device),        config_t::usbip_t::instance_count),
        string_multikey("usbip.%u.string.manufacturer",       LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.manufacturer), config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.product",            LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.product),      config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.serial",             LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.serial),       config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.midi",               LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.midi),         config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.hid",                LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.hid),          config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.uac",                LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.uac),          config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.cdc",                LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.cdc),          config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),
        string_multikey("usbip.%u.string.msc",                LM_INI_MULTIKEY_STR(config_rw.usbip[i].string_descriptors.msc),          config_t::usbip_t::instance_count, {.max_len = config_t::usbcommon::string_descriptor_max_len}),

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
