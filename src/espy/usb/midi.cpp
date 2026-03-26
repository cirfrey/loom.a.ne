#include "espy/usb/midi.hpp"

#include "espy/utils/math.hpp"

#include "tusb.h"

auto espy::usb::midi::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    /// TODO: SPLIT in-out
    if (cfg.midi == configuration_t::midi_inout) {
        auto audio_control_itf  = state.lowest_free_itf_idx++;
        auto midi_streaming_itf = state.lowest_free_itf_idx++;

        /// --- Securing endpoints ---
        // NOTE: EP IN and EP OUT share interfaces
        auto [ep_out_idx, ep_out]    = espy::usb::find_unassigned_ep(eps, dir_t::OUT, dir_t::INOUT);
        ep_out->configured_direction = ep_t::direction_t::OUT;
        ep_out->interface_type       = ep_t::interface_type_t::midi_streaming;
        ep_out->interface            = midi_streaming_itf;
        ep_out->type                 = ept_t::midi_bulk_out;

        auto [ep_in_idx, ep_in]     = espy::usb::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
        ep_in->configured_direction = ep_t::direction_t::IN;
        ep_in->interface_type       = ep_t::interface_type_t::midi_streaming;
        ep_in->interface            = midi_streaming_itf;
        ep_in->type                 = ept_t::midi_bulk_in;

        cfg.midi_cable_count = espy::math::clamp(cfg.midi_cable_count, 1, cfg.midi_max_cable_count);

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
void send_midi_note(uint8_t cable, uint8_t channel, uint8_t note, uint8_t velocity) {
    // 1. Construct the Header Byte
    // Upper 4 bits: Cable Number (0-15)
    // Lower 4 bits: Code Index Number (0x9 for Note On)
    uint8_t header = (uint8_t)((cable << 4) | 0x09);

    // 2. Construct the MIDI Payload
    uint8_t status   = (uint8_t)(0x90 | (channel & 0x0F));
    uint8_t note_num = (uint8_t)(note & 0x7F);
    uint8_t vel      = (uint8_t)(velocity & 0x7F);

    // 3. Create the 4-byte packet
    uint8_t packet[4] = { header, status, note_num, vel };

    // 4. Send to TinyUSB
    if (tud_midi_mounted()) { tud_midi_packet_write(packet); }
}
