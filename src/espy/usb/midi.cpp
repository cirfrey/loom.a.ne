#include "espy/usb/midi.hpp"

#include "tusb.h"

auto espy::usb::midi::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    /// TODO: SPLIT in-out
    if (cfg.midi == configuration_t::midi_inout) {
        auto audio_control_itf  = state.curr_itf_idx++;
        auto midi_streaming_itf = state.curr_itf_idx++;

        /// --- Securing endpoints ---
        // NOTE: EP IN and EP OUT share interfaces
        auto [ep_out_idx, ep_out]    = espy::usb::find_unassigned_ep(eps, dir_t::OUT, dir_t::INOUT);
        ep_out->configured_direction = ep_t::direction_t::OUT;
        ep_out->interface            = midi_streaming_itf;
        ep_out->interface_type       = ep_t::interface_type_t::midi_streaming;

        auto [ep_in_idx, ep_in]     = espy::usb::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
        ep_in->configured_direction = ep_t::direction_t::IN;
        ep_in->interface            = midi_streaming_itf;
        ep_in->interface_type       = ep_t::interface_type_t::midi_streaming;

        cfg.midi_cable_count = clamp(cfg.midi_cable_count, 1, cfg.midi_max_cable_count);

        /// --- Writing the descriptors ---
        /// If this is confusing refer to TUD_MIDI_DESCRIPTOR, thats the basic structure, this just has extra jacks.
        // Header.
        state.append_desc({ TUD_MIDI_DESC_HEAD(audio_control_itf, (u8)string_descriptor_idxs::idx_midi, cfg.midi_cable_count) });
        // Jacks.
        for(u8 i = 1; i <= cfg.midi_cable_count; i++) state.append_desc({ TUD_MIDI_DESC_JACK_DESC(i, (u8)((u8)string_descriptor_idxs::idx_max + i)) });
        // OUT EP + Mapping.
        state.append_desc({ TUD_MIDI_DESC_EP(ep_out_idx, CFG_TUD_MIDI_EP_BUFSIZE, cfg.midi_cable_count) });
        for(u8 i = 1; i <= cfg.midi_cable_count; i++) state.append_desc({ TUD_MIDI_JACKID_IN_EMB(i) });
        // IN EP + Mapping.
        state.append_desc({ TUD_MIDI_DESC_EP((u8)(EP_DIR_IN | ep_in_idx), CFG_TUD_MIDI_EP_BUFSIZE, cfg.midi_cable_count) });
        for(u8 i = 1; i <= cfg.midi_cable_count; i++) state.append_desc({ TUD_MIDI_JACKID_OUT_EMB(i) });
    }
}

extern "C"
{
    void tud_midi_rx_cb(uint8_t itf) { (void) itf; }
}
