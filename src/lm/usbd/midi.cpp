#include "lm/usbd/midi.hpp"

#include "tusb.h"

#include "lm/config.hpp"

#include "lm/core/math.hpp"
#include "lm/core/cvt.hpp"

auto lm::usbd::midi::do_configuration_descriptor(
    usb::configuration_descriptor_builder_state_t& state,
    std::span<usb::ep_t> eps,
    u8 cable_count,
    mode midi_mode,
    bool strict_eps
) -> st
{
    using namespace usb;

    /// TODO: SPLIT in-out
    if (midi_mode == mode::inout) {
        auto audio_control_itf  = state.lowest_free_itf_idx++;
        auto midi_streaming_itf = state.lowest_free_itf_idx++;

        auto ep_in_idx = 0_u8;
        auto ep_out_idx = 0_u8;
        if(strict_eps)
        {
            auto [ep_inout_idx, ep_inout] = find_unassigned_ep_inout(eps);
            ep_inout->out_itf_idx = midi_streaming_itf;
            ep_inout->out_itf     = itf_t::midi_streaming;
            ep_inout->out         = ept_t::midi_bulk_out;
            ep_inout->in_itf_idx  = midi_streaming_itf;
            ep_inout->in_itf      = itf_t::midi_streaming;
            ep_inout->in          = ept_t::midi_bulk_in;
            ep_in_idx             = ep_inout_idx;
            ep_out_idx            = ep_inout_idx;
        }
        else
        {
            auto [_ep_out_idx, ep_out] = find_unassigned_ep_out(eps);
            ep_out->out_itf_idx = midi_streaming_itf;
            ep_out->out_itf     = itf_t::midi_streaming;
            ep_out->out         = ept_t::midi_bulk_out;
            ep_out_idx          = _ep_out_idx;
            auto [_ep_in_idx, ep_in] = find_unassigned_ep_in(eps);
            ep_in->in_itf_idx = midi_streaming_itf;
            ep_in->in_itf     = itf_t::midi_streaming;
            ep_in->in         = ept_t::midi_bulk_in;
            ep_in_idx         = _ep_in_idx;
        }

        cable_count = clamp(cable_count, 1, config_t::midi_t::max_cables);

        auto appended = 0_st;
        /// --- Writing the descriptors ---
        /// If this is confusing refer to TUD_MIDI_DESCRIPTOR, thats the basic structure, this just has extra jacks.
        // Header.
        appended += state.append_desc({ TUD_MIDI_DESC_HEAD(audio_control_itf, string_descriptor::midi, cable_count) });
        // Jacks.
        for(u8 i = 1; i <= cable_count; i++)
            // TODO: make it so the string index of these descriptors can be a custom value instead of the default
            //       'expect theres a section for midi jack string descriptors after the main ones'.
            // TODO: should the -1 be here? (u8)(string_descriptor::midi_cable_start + i - 1
            appended += state.append_desc({ TUD_MIDI_DESC_JACK_DESC(i, (u8)(string_descriptor::midi_cable_start + i - 1)) });
        // OUT EP + Mapping.
        appended += state.append_desc({ TUD_MIDI_DESC_EP(ep_out_idx, CFG_TUD_MIDI_EP_BUFSIZE, cable_count) });
        for(u8 i = 1; i <= cable_count; i++)
            appended += state.append_desc({ TUD_MIDI_JACKID_IN_EMB(i) });
        // IN EP + Mapping.
        appended += state.append_desc({ TUD_MIDI_DESC_EP((u8)(EP_DIR_IN | ep_in_idx), CFG_TUD_MIDI_EP_BUFSIZE, cable_count) });
        for(u8 i = 1; i <= cable_count; i++)
            appended += state.append_desc({ TUD_MIDI_JACKID_OUT_EMB(i) });

        return appended;
    }

    return 0;
}

/// TODO: moveme!
extern "C"
{
    void tud_midi_rx_cb(uint8_t itf) { (void) itf; }
}

void send_midi_note(lm::u8 cable, lm::u8 channel, lm::u8 note, lm::u8 velocity) {
    using namespace lm;

    usbd::midi::packet pkt({
        .cable    = cable,
        .code     = usbd::midi::cin::note_on,
        .channel  = channel,
        .type     = usbd::midi::status::note_on,
        .note     = note,
        .velocity = velocity
    });

    if (tud_midi_mounted()) { tud_midi_packet_write(&pkt | rc<u8*>); }
}
