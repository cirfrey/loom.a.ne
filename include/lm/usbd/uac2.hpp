#pragma once

#include "lm/strands/usbd.hpp"
#include "lm/usbd/common.hpp"

namespace lm::usbd::uac2
{
    auto do_configuration_descriptor(
        configuration_descriptor_builder_state_t& s,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void;
}
