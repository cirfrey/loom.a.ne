#pragma once

#include "lm/usb/common.hpp"

namespace lm::usbd::hid
{
    auto do_configuration_descriptor(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        u8 pool_interval_ms
    ) -> st;

    namespace hid_reportid
    {
        enum hid_reportid_t : u8
        {
            mouse    = 1,
            keyboard = 2,
            gamepad  = 3, // Standard 16-button, 2-joystick, 8-way hat gamepad.
            vendor   = 4  // See configuration_t::logging_t::hid_vendor_logging.
        };
    }
}
