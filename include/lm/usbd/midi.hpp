#pragma once

#include "lm/tasks/usbd.hpp"
#include "lm/usbd/common.hpp"

namespace lm::usbd::midi
{
    auto do_configuration_descriptor(
        configuration_descriptor_builder_state_t& s,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void;
}
