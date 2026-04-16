#pragma once

#include "lm/fabric.hpp"
#include "lm/config.hpp"

#include "lm/usb/common.hpp"

#include <common/tusb_types.h>

#include <array>

namespace lm::strands
{
    struct usbd
    {
        struct event
        {
            enum event_t : u8
            {
                get_status,

                cdc_enabled,
                cdc_disabled,
                hid_enabled,
                hid_disabled
            };
        };

        usbd(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~usbd();

        auto broadcast_status() -> void;

        std::array<
            config_t::usbcommon::string_descriptor,
            usb::string_descriptor::count + config_t::midi_t::max_cables
        > string_descriptors;
        tusb_desc_device_t device_descriptor = {
            .bLength            = sizeof(tusb_desc_device_t),
            .bDescriptorType    = TUSB_DESC_DEVICE,
            .bcdUSB             = 0x0200,      // USB 2.0
            .bDeviceClass       = TUSB_CLASS_MISC,
            .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
            .bDeviceProtocol    = MISC_PROTOCOL_IAD,
            .bMaxPacketSize0    = 64, // TODO: review.
            .idVendor           = 0x0000,
            .idProduct          = 0x0000,
            .bcdDevice          = 0x0000,
            .iManufacturer      = usb::string_descriptor::manufacturer,
            .iProduct           = usb::string_descriptor::product,
            .iSerialNumber      = usb::string_descriptor::serial,
            .bNumConfigurations = 0x01         // We only have 1 "floor plan"
        };
        u8 config_descriptor[config_t::usb_t::config_descriptor_max_size] = {0};

        fabric::queue_t get_status_q;
        fabric::bus::subscribe_token get_status_q_tok;

        fabric::strand::handle_t tud_strand_handle = nullptr;
    };
}
