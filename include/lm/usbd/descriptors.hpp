#pragma once

#include "lm/usb/common.hpp"

#include <span>

namespace lm::usbd::descriptor
{
    auto audio(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        u8 microphone_channels,
        u8 speaker_channels
    ) -> st;

    auto cdc(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        bool strict_eps
    ) -> st;

    auto hid(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        u8 pool_interval_ms
    ) -> st;

    enum class midi_mode {
        in,
        out,
        inout,
    };
    auto midi(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        u8 cable_count,
        midi_mode _midi_mode,
        bool strict_eps
    ) -> st;

    auto msc(
        usb::configuration_descriptor_builder_state_t& state,
        std::span<usb::ep_t> eps,
        bool strict_eps
    ) -> st;
}
