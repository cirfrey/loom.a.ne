#pragma once

#include "lm/tasks/usbd.hpp"
#include "lm/usbd/common.hpp"

namespace lm::usbd
{
    inline std::array<ep_t, 7> esp32_endpoints = {{
        { .type = ep_t::type_t::control,    .interface = 0, .interface_type = ep_t::interface_type_t::control,    .available_directions = ep_t::direction_t::INOUT, .configured_direction = ep_t::direction_t::INOUT },
        { .type = ep_t::type_t::unassigned, .interface = 0, .interface_type = ep_t::interface_type_t::unassigned, .available_directions = ep_t::direction_t::INOUT, .configured_direction = ep_t::direction_t::NONE },
        { .type = ep_t::type_t::unassigned, .interface = 0, .interface_type = ep_t::interface_type_t::unassigned, .available_directions = ep_t::direction_t::INOUT, .configured_direction = ep_t::direction_t::NONE },
        { .type = ep_t::type_t::unassigned, .interface = 0, .interface_type = ep_t::interface_type_t::unassigned, .available_directions = ep_t::direction_t::INOUT, .configured_direction = ep_t::direction_t::NONE },
        { .type = ep_t::type_t::unassigned, .interface = 0, .interface_type = ep_t::interface_type_t::unassigned, .available_directions = ep_t::direction_t::INOUT, .configured_direction = ep_t::direction_t::NONE },
        { .type = ep_t::type_t::unassigned, .interface = 0, .interface_type = ep_t::interface_type_t::unassigned, .available_directions = ep_t::direction_t::INOUT, .configured_direction = ep_t::direction_t::NONE },
        { .type = ep_t::type_t::unassigned, .interface = 0, .interface_type = ep_t::interface_type_t::unassigned, .available_directions = ep_t::direction_t::IN   , .configured_direction = ep_t::direction_t::NONE }
    }};
}
