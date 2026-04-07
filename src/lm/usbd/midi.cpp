#include "lm/usbd/midi.hpp"

#include "tusb.h"

#if CFG_TUD_MIDI == 0

    #include "lm/log.hpp"
    auto lm::usbd::midi::do_configuration_descriptor(
        configuration_descriptor_builder_state_t& state,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void {
        cfg.midi = cfg.no_midi;
        log::debug("MIDI disabled via CFG_TUD_MIDI=0\n");
    }

#else

#include "lm/core/math.hpp"
#include "lm/core/cvt.hpp"

auto lm::usbd::midi::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    /// TODO: SPLIT in-out
    if (cfg.midi == configuration_t::midi_inout) {
        auto audio_control_itf  = state.lowest_free_itf_idx++;
        auto midi_streaming_itf = state.lowest_free_itf_idx++;

        auto ep_in_idx = 0_u8;
        auto ep_out_idx = 0_u8;
        if(cfg.midi_strict_endpoints)
        {
            auto [ep_inout_idx, ep_inout] = lm::usbd::find_unassigned_ep_inout(eps);
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
            auto [_ep_out_idx, ep_out] = lm::usbd::find_unassigned_ep_out(eps);
            ep_out->out_itf_idx = midi_streaming_itf;
            ep_out->out_itf     = itf_t::midi_streaming;
            ep_out->out         = ept_t::midi_bulk_out;
            ep_out_idx          = _ep_out_idx;
            auto [_ep_in_idx, ep_in] = lm::usbd::find_unassigned_ep_in(eps);
            ep_in->in_itf_idx = midi_streaming_itf;
            ep_in->in_itf     = itf_t::midi_streaming;
            ep_in->in         = ept_t::midi_bulk_in;
            ep_in_idx         = _ep_in_idx;
        }

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

/// TODO: API me!
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

#endif
