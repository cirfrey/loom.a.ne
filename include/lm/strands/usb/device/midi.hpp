#pragma once

#include "lm/fabric/all.hpp"
#include "lm/config.hpp"
#include "lm/usb/common.hpp"

#include <common/tusb_types.h>

namespace lm::strands::usb::device
{
    struct midi
    {
        using ri     = fabric::strand::strand_runtime_info;
        using status = fabric::strand::managed_strand_status;

        midi(ri& info, config_t::midi_t_ const* cfg = nullptr);
        auto on_wake() -> status;
        ~midi() = default;

        ri& info;
        config_t::midi_t_ const* cfg;

        // Learned at runtime from the first capability {announcing, transport} event.
        // Reset to 0 on capability {detach}.
        u8   transport_strand_id = 0;

        // Own endpoint table — filled by the descriptor builder in the constructor.
        // EP0 is implicit (control), so we only need bulk IN + bulk OUT here.
        static constexpr u8 ep_count = 3; // 0=control, 1=bulk_in, 2=bulk_out
        usb::ep_t eps[ep_count];
        u8 bulk_in_ep  = 0; // filled after descriptor build
        u8 bulk_out_ep = 0;

        // Device descriptor — fixed for MIDI.
        tusb_desc_device_t device_descriptor;

        // Config descriptor — built once in constructor.
        u8 config_descriptor[config_t::midi_t_::config_descriptor_max_size];

        // Bus subscription — framework topic, filtered to usb_ctrl_* types addressed to us.
        fabric::queue_t              q;
        fabric::bus::subscribe_token tok;

        // Handlers — called from on_wake.
        auto handle_capability      (fabric::event const&) -> void;
        auto handle_setup_request   (fabric::event const&) -> void;
        auto handle_data            (fabric::event const&) -> void;
        auto handle_interface_status(fabric::event const&) -> void;

        // Publish usb_ctrl_data {in} to transport for a given ep and payload.
        // Handles splitting into extensions as needed.
        auto send_data_to_transport(u8 ep, u8 const* data, u16 len) -> void;
    };
}
