#include "lm/tasks/usbd.hpp"

#include "lm/usbd/common.hpp"
#include "lm/usbd/cdc.hpp"
#include "lm/usbd/midi.hpp"
#include "lm/usbd/msc.hpp"
#include "lm/usbd/hid.hpp"
#include "lm/usbd/uac2.hpp"

#include "lm/chip/info.hpp"
#include "lm/chip/usb.hpp"
#include "lm/config.hpp"
#include "lm/board.hpp"
#include "lm/fabric/task.hpp"
#include "lm/fabric/bus.hpp"

#include <array>

#include <tusb.h>


// Static stuff required for this to work.
namespace lm::usbd
{
    constexpr auto string_descriptor_max_size = 32;
    std::array<char[string_descriptor_max_size], (u8)string_descriptor_idxs::idx_max + configuration_t::midi_max_cable_count> string_descriptor_arr;

    static tusb_desc_device_t device_descriptor = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,      // USB 2.0

        // --- CRITICAL FOR COMPOSITE DEVICES ---
        // Use 0xEF / 0x02 / 0x01 (Miscellaneous / Common / Interface Association)
        // This tells Windows/Linux/Mac: "Look at the IADs in the config descriptor
        // to figure out which drivers to load for each part."
        .bDeviceClass       = TUSB_CLASS_MISC,
        .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol    = MISC_PROTOCOL_IAD,

        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE, // Usually 64

        // Refer to https://github.com/espressif/usb-pids.
        .idVendor           = 0x303A,      // The primary USB Vendor ID (VID) for Espressif Systems.
        .idProduct          = 0x80C4,      // Lolin S2 Mini - UF2 Bootloader
        .bcdDevice          = 0x0100,      // Device Release Number (1.0)

        .iManufacturer      = (u8)string_descriptor_idxs::idx_manufacturer,
        .iProduct           = (u8)string_descriptor_idxs::idx_product,
        .iSerialNumber      = (u8)string_descriptor_idxs::idx_serial,

        .bNumConfigurations = 0x01         // We only have 1 "floor plan"
    };

    static uint8_t configuration_descriptor[lm::config::usb::config_descriptor_max_size];
}


auto lm::usbd::init(
    configuration_t& cfg,
    std::span<ep_t> eps,
    descriptors_t descriptors
) -> void
{
    /// TODO: load configuration state.

    // NOTE: string_descriptor_idx::idx_lang is handled by the tinyusb callback.
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_manufacturer], string_descriptor_max_size, descriptors.manufacturer);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_product],      string_descriptor_max_size, descriptors.product);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_serial],       string_descriptor_max_size, "%.*s", lm::chip::info::uuid().size, lm::chip::info::uuid().data);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_midi],         string_descriptor_max_size, descriptors.midi);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_hid],          string_descriptor_max_size, descriptors.hid);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_uac],          string_descriptor_max_size, descriptors.uac);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_cdc],          string_descriptor_max_size, descriptors.cdc);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_msc],          string_descriptor_max_size, descriptors.msc);
    // Add some more string descriptors for each extra MIDI cable.
    for(u8 i = 0; i < configuration_t::midi_max_cable_count; ++i) {
        snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_max + i],  string_descriptor_max_size, "%s - Jack %i", descriptors.midi, i);
    }
    device_descriptor.idVendor  = descriptors.vendor_id;
    device_descriptor.idProduct = descriptors.product_id;
    device_descriptor.bcdDevice = descriptors.bcd_device;

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

    // TODO: check config and if malconfigured enable only CDC and log out the reason.
    auto state = configuration_descriptor_builder_state_t{
        .desc = configuration_descriptor,
        .lowest_free_itf_idx = 0,
        .desc_curr_len = 0,
        .desc_max_len = lm::config::usb::config_descriptor_max_size,
    };
    // Configuration Header
    state.append_desc({ 0x09, TUSB_DESC_CONFIGURATION, 0, 0, 0, 1, 0, 0x80, 0x64 });
    // The composite classes.
    lm::usbd::cdc::do_configuration_descriptor(state, cfg, eps);
    lm::usbd::hid::do_configuration_descriptor(state, cfg, eps);
    lm::usbd::midi::do_configuration_descriptor(state, cfg, eps);
    lm::usbd::msc::do_configuration_descriptor(state, cfg, eps);
    lm::usbd::uac2::do_configuration_descriptor(state, cfg, eps);
    // Fixup header: Total Length and Interface Count
    configuration_descriptor[2] = (uint8_t)(state.desc_curr_len & 0xFF);
    configuration_descriptor[3] = (uint8_t)((state.desc_curr_len >> 8) & 0xFF);
    configuration_descriptor[4] = state.lowest_free_itf_idx;
}

// --- TinyUSB callbacks ---
extern "C" {

// Device Descriptor.
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &lm::usbd::device_descriptor;
}

// Configuration Descriptor.
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    return lm::usbd::configuration_descriptor;
}

// String Descriptors.
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    static uint16_t _desc_str[lm::usbd::string_descriptor_max_size]; // Shared buffer for string descriptors

    // --- Index 0 is NOT a string, it's the Language ID list ---
    if (index == (lm::u8)lm::usbd::string_descriptor_idxs::idx_lang) {
        static uint16_t const lang_id[] = { (uint16_t)((TUSB_DESC_STRING << 8) | 0x04), 0x0409 };
        return lang_id;
    }

    // Helper to convert UTF-8 (char*) to UTF-16 (uint16_t)
    auto utf8_to_utf16 = [](const char* str) -> uint16_t* {
        if (!str) str = "";

        uint8_t chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        // First byte is Length (Total bytes including header), Second is Type (0x03)
        // In TinyUSB, we return this as a uint16_t array where the first element
        // packs these two bytes: (0x03 << 8) | (Length)
        _desc_str[0] = (uint16_t)((0x03 << 8) | (2 * chr_count + 2));

        // Convert ASCII/UTF8 chars to UTF16 (simple cast for standard ASCII)
        for (uint8_t i = 0; i < chr_count; i++) { _desc_str[1 + i] = str[i]; }

        return _desc_str;
    };
    // Return the UTF-16 string for the given index
    return utf8_to_utf16( lm::usbd::string_descriptor_arr[index] );
}

void tud_mount_cb(void) { }
void tud_umount_cb(void) { }
void tud_suspend_cb(bool remote_wakeup_en) { (void) remote_wakeup_en; }
void tud_resume_cb(void) { }

} // extern "C"
