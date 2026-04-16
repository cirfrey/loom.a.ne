#pragma once

#include "lm/strands/usbd.hpp"
#include "lm/usbd/common.hpp"

namespace lm::usbd
{
    inline std::array<ep_t, 7> esp32_endpoints = {{
        {
            .in = ept_t::control,
            .in_itf = itf_t::control,
            .out = ept_t::control,
            .out_itf = itf_t::control
        },

        { .in = ept_t::unassigned, .out = ept_t::unassigned },
        { .in = ept_t::unassigned, .out = ept_t::unassigned },
        { .in = ept_t::unassigned, .out = ept_t::unassigned },
        { .in = ept_t::unassigned, .out = ept_t::unassigned },
        { .in = ept_t::unassigned, .out = ept_t::unassigned },
        { .in = ept_t::unassigned, .out = ept_t::unavailable },
    }};
}
