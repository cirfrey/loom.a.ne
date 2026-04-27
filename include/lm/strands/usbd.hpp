#pragma once

#include "lm/fabric/all.hpp"
#include "lm/config.hpp"

#include "lm/usb/common.hpp"

#include <common/tusb_types.h>

#include <array>

namespace lm::strands
{
    struct usbd
    {
        using ri     = fabric::strand::strand_runtime_info;
        using status = fabric::strand::managed_strand_status;

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

        usbd(ri& info);
        auto on_ready()     -> status;
        auto before_sleep() -> status;
        auto on_wake()      -> status;
        ~usbd();

        std::array<
            config_t::usbcommon::string_descriptor,
            usb::string_descriptor::count
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
        u16 config_descriptor_size = 0; // The actual size of the descriptor.

        fabric::strand::handle_t tud_strand_handle = nullptr;
    };
}
