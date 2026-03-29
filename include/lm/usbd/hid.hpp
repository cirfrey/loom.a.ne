#pragma once

#include "lm/tasks/usbd.hpp"
#include "lm/usbd/common.hpp"

namespace lm::usbd::hid
{
    auto do_configuration_descriptor(
        configuration_descriptor_builder_state_t& s,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void;

    enum class hid_reportid : u8
    {
        mouse    = 1,
        keyboard = 2,
        gamepad  = 3, // Standard 16-button, 2-joystick, 8-way hat gamepad.
        vendor   = 4  // See configuration_t::logging_t::hid_vendor_logging.
    };
}
