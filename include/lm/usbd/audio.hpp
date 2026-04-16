#pragma once

#include "lm/usb/common.hpp"

#include <span>

namespace lm::usbd::audio
{
    auto do_configuration_descriptor(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        u8 microphone_channels,
        u8 speaker_channels
    ) -> st;
}
