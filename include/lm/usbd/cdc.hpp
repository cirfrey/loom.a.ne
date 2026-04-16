#pragma once

#include "lm/usb/common.hpp"

namespace lm::usbd::cdc
{
    auto do_configuration_descriptor(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        bool strict_eps
    ) -> st;
}
