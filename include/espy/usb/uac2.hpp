#pragma once

#include "espy/usb/usb.hpp"
#include "espy/usb/common.hpp"

namespace espy::usb::uac2
{
    auto do_configuration_descriptor(
        configuration_descriptor_builder_state_t& s,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void;
}
