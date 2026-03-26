#include "espy/usb/usb.hpp"

#include "espy/usb/common.hpp"
#include "espy/usb/cdc.hpp"
#include "espy/usb/midi.hpp"
#include "espy/usb/msc.hpp"
#include "espy/usb/hid.hpp"
#include "espy/usb/uac2.hpp"
#include "espy/config.hpp"
#include "espy/task.hpp"
#include "espy/bus.hpp"

#include <array>

#include <tusb.h>

#include <esp_log.h>
#include <esp_mac.h>
#include <esp_private/usb_phy.h>
#include <driver/gpio.h>

// Static stuff required for this to work.
namespace espy::usb::inline impl
{
    constexpr auto string_descriptor_max_size = 32;
    std::array<char[string_descriptor_max_size], (u8)string_descriptor_idxs::idx_max + configuration_t::midi_max_cable_count> string_descriptor_arr;

    static usb_phy_handle_t phy_hdl;
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

    static uint8_t configuration_descriptor[espy::config::usb::config_descriptor_max_size];
}

auto espy::usb::init(
    configuration_t& cfg,
    std::span<ep_t> eps,
    descriptors_t descriptors
) -> void
{
    // NOTE: string_descriptor_idx::idx_lang is handled by the tinyusb callback.
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_manufacturer], string_descriptor_max_size, descriptors.manufacturer);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_product],      string_descriptor_max_size, descriptors.product);
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    snprintf(string_descriptor_arr[(u8)string_descriptor_idxs::idx_serial],       string_descriptor_max_size, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

    // TODO: check config and if malconfigured enable only CDC and log out the reason.
    auto state = configuration_descriptor_builder_state_t{
        .desc = configuration_descriptor,
        .lowest_free_itf_idx = 0,
        .desc_curr_len = 0,
        .desc_max_len = espy::config::usb::config_descriptor_max_size,
    };
    // Configuration Header
    state.append_desc({ 0x09, TUSB_DESC_CONFIGURATION, 0, 0, 0, 1, 0, 0x80, 0x64 });
    // The composite classes.
    espy::usb::cdc::do_configuration_descriptor(state, cfg, eps);
    espy::usb::hid::do_configuration_descriptor(state, cfg, eps);
    espy::usb::midi::do_configuration_descriptor(state, cfg, eps);
    espy::usb::msc::do_configuration_descriptor(state, cfg, eps);
    espy::usb::uac2::do_configuration_descriptor(state, cfg, eps);
    // Fixup header: Total Length and Interface Count
    configuration_descriptor[2] = (uint8_t)(state.desc_curr_len & 0xFF);
    configuration_descriptor[3] = (uint8_t)((state.desc_curr_len >> 8) & 0xFF);
    configuration_descriptor[4] = state.lowest_free_itf_idx;

    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,      // Use the OTG controller
        .target = USB_PHY_TARGET_INT,        // Internal PHY pins (GPIO 19/20)
        .otg_mode = USB_OTG_MODE_DEVICE,     // Explicitly set as a Device
        .otg_speed = USB_PHY_SPEED_FULL,     // 12 Mbps
        .ext_io_conf = nullptr,              // Not using external PHY
        .otg_io_conf = nullptr               // Use default IO pins
    };
    ESP_ERROR_CHECK(usb_new_phy(&phy_conf, &espy::usb::phy_hdl));

    if(cfg.cdc) espy::bus::publish({.topic = espy::bus::usb, .type = (u8)espy::usb::event::cdc_enabled});
    else        espy::bus::publish({.topic = espy::bus::usb, .type = (u8)espy::usb::event::cdc_disabled});
    if(cfg.hid) espy::bus::publish({.topic = espy::bus::usb, .type = (u8)espy::usb::event::hid_enabled});
    else        espy::bus::publish({.topic = espy::bus::usb, .type = (u8)espy::usb::event::hid_disabled});

    espy::task::create(espy::config::task::usb_device, [](void *tparams){
        tusb_init(); // This will trigger the descriptor callbacks immediately.
        while (1) {
            tud_task(); // Device event handling loop
            espy::task::delay_ms(espy::config::task::usb_device.sleep_ms); // Yield to other tasks
        }
    });
}

// --- TinyUSB callbacks ---
extern "C" {

// Device Descriptor.
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &espy::usb::device_descriptor;
}

// Configuration Descriptor.
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    return espy::usb::configuration_descriptor;
}

// String Descriptors.
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    static uint16_t _desc_str[espy::usb::string_descriptor_max_size]; // Shared buffer for string descriptors

    // --- Index 0 is NOT a string, it's the Language ID list ---
    if (index == (espy::u8)espy::usb::string_descriptor_idxs::idx_lang) {
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
    return utf8_to_utf16( espy::usb::string_descriptor_arr[index] );
}

void tud_mount_cb(void) { }
void tud_umount_cb(void) { }
void tud_suspend_cb(bool remote_wakeup_en) { (void) remote_wakeup_en; }
void tud_resume_cb(void) { }

} // extern "C"
