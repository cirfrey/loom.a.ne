#pragma once

#include "lm/usb/common.hpp"

namespace lm::arch::esp32s2
{
    inline std::array<usb::ep_t, 7> endpoints = {{
        {
            .in      = usb::ept_t::control,
            .in_itf  = usb::itf_t::control,
            .out     = usb::ept_t::control,
            .out_itf = usb::itf_t::control
        },

        { .in = usb::ept_t::unassigned, .out = usb::ept_t::unassigned },
        { .in = usb::ept_t::unassigned, .out = usb::ept_t::unassigned },
        { .in = usb::ept_t::unassigned, .out = usb::ept_t::unassigned },
        { .in = usb::ept_t::unassigned, .out = usb::ept_t::unassigned },
        { .in = usb::ept_t::unassigned, .out = usb::ept_t::unassigned },
        { .in = usb::ept_t::unassigned, .out = usb::ept_t::unavailable },
    }};
}
