// A helper to have a single function setup the descriptors
// for usb and usbip backends.
#pragma once

#include "lm/core/types.hpp"
#include "lm/chip/info.hpp"

#include "lm/config.hpp"

#include "lm/usbd/audio.hpp"
#include "lm/usbd/cdc.hpp"
#include "lm/usbd/hid.hpp"
#include "lm/usbd/midi.hpp"
#include "lm/usbd/msc.hpp"

#include <cstdio>

namespace lm::usb::backend
{
    struct descriptor_lengths
    {
        st header = 0;

        st audio = 0;
        st cdc = 0;
        st hid = 0;
        st midi = 0;
        st msc = 0;

        st total = 0;
    };
    constexpr auto setup_descriptors(
        auto& config_descriptor,
        auto& string_descriptors,
        auto& device_descriptor,
        // These point to specific config_t members.
        auto& cfg,
        auto& audio,
        auto& cdc,
        auto& hid,
        auto& midi,
        auto& msc
    ) -> descriptor_lengths;
}


constexpr auto lm::usb::backend::setup_descriptors(
    auto& config_descriptor,
    auto& string_descriptors,
    auto& device_descriptor,
    auto& cfg,
    auto& audio,
    auto& cdc,
    auto& hid,
    auto& midi,
    auto& msc
) -> descriptor_lengths
{
    auto write_strdsc = [&](auto idx, auto fmt, auto... args){
        return std::snprintf(
            string_descriptors[idx],
            config_t::usbcommon::string_descriptor_max_len,
            fmt,
            veil::forward<decltype(args)>(args)...
        );
    };

    // NOTE: string_descriptor::lang is handled by the tinyusb callback.
    write_strdsc(string_descriptor::manufacturer, "%s", cfg.string_descriptors.manufacturer);
    write_strdsc(string_descriptor::product,      "%s", cfg.string_descriptors.product);
    write_strdsc(string_descriptor::serial,       "%.*s", (int)chip::info::uuid().size, chip::info::uuid().data);
    write_strdsc(string_descriptor::midi,         "%s", cfg.string_descriptors.midi);
    write_strdsc(string_descriptor::hid,          "%s", cfg.string_descriptors.hid);
    write_strdsc(string_descriptor::uac,          "%s", cfg.string_descriptors.uac);
    write_strdsc(string_descriptor::cdc,          "%s", cfg.string_descriptors.cdc);
    write_strdsc(string_descriptor::msc,          "%s", cfg.string_descriptors.msc);
    // Add some more string descriptors for each extra MIDI cable.
    for(u8 i = 0; i < config_t::midi_t::max_cables; ++i) {
        write_strdsc(string_descriptor::midi_cable_start + i, "%s - Cable %i", cfg.string_descriptors.midi, i);
    }

    device_descriptor.bDeviceClass    = cfg.device_descriptor.device_class;
    device_descriptor.bDeviceSubClass = cfg.device_descriptor.device_subclass;
    device_descriptor.bDeviceProtocol = cfg.device_descriptor.device_protocol;
    device_descriptor.idVendor        = cfg.device_descriptor.vendor_id;
    device_descriptor.idProduct       = cfg.device_descriptor.product_id;
    device_descriptor.bcdDevice       = cfg.device_descriptor.bcd_device;

    /// TODO: verify this!
    // If we have CDC (or UAC2 Audio), we have IADs and MUST use the MISC class.
    // If we only have HID or MSC, we MUST use 0x00 so the OS loads the interface drivers directly.
    // if (cfg.cdc || cfg.uac != configuration_t::no_uac) {
    //     device_descriptor.bDeviceClass    = TUSB_CLASS_MISC;
    //     device_descriptor.bDeviceSubClass = MISC_SUBCLASS_COMMON;
    //     device_descriptor.bDeviceProtocol = MISC_PROTOCOL_IAD;
    // } else {
    //     device_descriptor.bDeviceClass    = 0x00; // TUSB_CLASS_UNSPECIFIED
    //     device_descriptor.bDeviceSubClass = 0x00;
    //     device_descriptor.bDeviceProtocol = 0x00;
    // }

    auto state = configuration_descriptor_builder_state_t{
        .desc = config_descriptor,
        .lowest_free_itf_idx = 0,
        .desc_curr_len = 0,
        .desc_max_len = sizeof(config_descriptor),
    };

    auto lens = descriptor_lengths{};

    // Configuration Header

    lens.header = state.append_desc({ 0x09, TUSB_DESC_CONFIGURATION, 0, 0, 0, 1, 0, 0x80, 0x64 });

    // The composite classes.

    lens.cdc = cdc.toggle == feature::on
        ? lm::usbd::cdc::do_configuration_descriptor(state,  cfg.endpoints, cdc.strict_eps == feature::on)
        : 0;

    lens.hid = hid.toggle == feature::on
        ? lm::usbd::hid::do_configuration_descriptor(state,  cfg.endpoints, hid.pool_ms)
        : 0;

    constexpr auto map_midi_mode = [](config_t::midi_t::mode_t m) -> lm::usbd::midi::mode {
        if     (m == config_t::midi_t::in)  return lm::usbd::midi::mode::in;
        else if(m == config_t::midi_t::out) return lm::usbd::midi::mode::out;
        else                                return lm::usbd::midi::mode::inout;
    };
    lens.midi = midi.cable_count > 0
        ? lm::usbd::midi::do_configuration_descriptor(
            state,
            cfg.endpoints,
            midi.cable_count,
            map_midi_mode(midi.mode),
            midi.strict_eps == feature::on
        )
        : 0;

    lens.msc = msc.toggle == feature::on
        ? lm::usbd::msc::do_configuration_descriptor(state,  cfg.endpoints, msc.strict_eps == feature::on)
        : 0;

    lens.audio = audio.microphone_channels > 0 || audio.speaker_channels > 0
        ? lm::usbd::audio::do_configuration_descriptor(
            state,
            cfg.endpoints,
            audio.microphone_channels,
            audio.speaker_channels
        )
        : 0;

    // Fixup header: Total Length and Interface Count
    config_descriptor[2] = (u8)(state.desc_curr_len & 0xFF);
    config_descriptor[3] = (u8)((state.desc_curr_len >> 8) & 0xFF);
    config_descriptor[4] = state.lowest_free_itf_idx;

    lens.total = state.desc_curr_len;

    return lens;
}
