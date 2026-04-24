#include "lm/usbd/descriptors.hpp"

#include "lm/config.hpp"
#include "lm/core/math.hpp"

#include "lm/lib/hid.hpp"

#include "tusb.h"

auto lm::usbd::descriptor::audio(
    usb::configuration_descriptor_builder_state_t& state,
    std::span<usb::ep_t> eps,
    u8 microphone_channels,
    [[maybe_unused]] u8 speaker_channels
) -> st
{
    using namespace usb;

    if (microphone_channels) {
        auto const control_itf = state.lowest_free_itf_idx++;
        auto const streaming_itf = state.lowest_free_itf_idx++;

        auto [ep_in_idx, ep_in] = find_unassigned_ep_in(eps);
        ep_in->in_itf     = itf_t::uac2_streaming;
        ep_in->in_itf_idx = streaming_itf;
        ep_in->in         = ept_t::uac2_iso_in;

        // 3. Append Descriptor
        // TUD_AUDIO_DESC_IAD(itfnum, str_idx)
        // TUD_AUDIO_DESC_CONTROL(itfnum, str_idx, revision)
        // TUD_AUDIO_DESC_ST_IN(itf_stream, str_idx, n_channels, resolution, ep_in)
        return state.append_desc({ TUD_AUDIO20_MIC_ONE_CH_DESCRIPTOR(
            control_itf,
            string_descriptor::uac,
            CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
            CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX*8,
            (u8)(EP_DIR_IN | ep_in_idx),
            CFG_TUD_AUDIO_EP_SZ_IN
        )});
    }

    return 0;
}

auto lm::usbd::descriptor::cdc(
    usb::configuration_descriptor_builder_state_t& state,
    std::span<usb::ep_t> eps,
    bool strict_eps
) -> st
{
    using namespace usb;

    auto cdc_comm_itf = state.lowest_free_itf_idx++;
    auto cdc_data_itf = state.lowest_free_itf_idx++;

    auto [ep_notif_idx, ep_notif] = find_unassigned_ep_in(eps);
    ep_notif->in_itf_idx = cdc_comm_itf;
    ep_notif->in_itf     = itf_t::cdc_comm;
    ep_notif->in         = ept_t::cdc_interrupt_in;

    auto ep_out_idx = 0_u8;
    auto ep_in_idx  = 0_u8;
    if(strict_eps)
    {
        auto [ep_inout_idx, ep_inout] = find_unassigned_ep_inout(eps);
        ep_inout->out_itf_idx = cdc_data_itf;
        ep_inout->out_itf     = itf_t::cdc_data;
        ep_inout->out         = ept_t::cdc_bulk_out;
        ep_inout->in_itf_idx  = cdc_data_itf;
        ep_inout->in_itf      = itf_t::cdc_data;
        ep_inout->in          = ept_t::cdc_bulk_in;
        ep_in_idx             = ep_inout_idx;
        ep_out_idx            = ep_inout_idx;
    }
    else
    {
        auto [_ep_out_idx, ep_out] = find_unassigned_ep_out(eps);
        ep_out->out_itf_idx = cdc_data_itf;
        ep_out->out_itf     = itf_t::midi_streaming;
        ep_out->out         = ept_t::cdc_bulk_out;
        ep_out_idx          = _ep_out_idx;
        auto [_ep_in_idx, ep_in] = find_unassigned_ep_in(eps);
        ep_in->in_itf_idx = cdc_data_itf;
        ep_in->in_itf     = itf_t::midi_streaming;
        ep_in->in         = ept_t::cdc_bulk_in;
        ep_in_idx         = _ep_in_idx;
    }

    return state.append_desc({ TUD_CDC_DESCRIPTOR(
        cdc_comm_itf, // first itfnum (next one is itfnum+1).
        string_descriptor::cdc, // stridx.
        (u8)(EP_DIR_IN | ep_notif_idx), // ep_notif.
        8, // ep_notif_size.
        ep_out_idx, // epout.
        (u8)(EP_DIR_IN | ep_in_idx), // epin.
        CFG_TUD_CDC_EP_BUFSIZE // ep_size.
    )});
}

auto lm::usbd::descriptor::hid(
    usb::configuration_descriptor_builder_state_t& state,
    std::span<usb::ep_t> eps,
    u8 pool_interval_ms
) -> st
{
    using namespace usb;

    auto hid_itf = state.lowest_free_itf_idx++;

    auto [ep_in_idx, ep_in] = find_unassigned_ep_in(eps);
    ep_in->in_itf_idx = hid_itf;
    ep_in->in_itf     = itf_t::hid;
    ep_in->in         = ept_t::hid_interrupt_in;

    return state.append_desc({ TUD_HID_DESCRIPTOR(
        hid_itf,
        string_descriptor::hid,
        HID_ITF_PROTOCOL_NONE,
        sizeof(lm::hid::report_descriptor),
        (u8)(EP_DIR_IN | ep_in_idx), // epin.
        64, // epsize.
        pool_interval_ms
    )});
}

auto lm::usbd::descriptor::midi(
    usb::configuration_descriptor_builder_state_t& state,
    std::span<usb::ep_t> eps,
    u8 cable_count,
    midi_mode midi_mode,
    bool strict_eps
) -> st
{
    using namespace usb;

    /// TODO: SPLIT in-out
    if (midi_mode == midi_mode::inout) {
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


auto lm::usbd::descriptor::msc(
    usb::configuration_descriptor_builder_state_t& state,
    std::span<usb::ep_t> eps,
    bool strict_eps
) -> st
{
    using namespace usb;

    auto msc_itf = state.lowest_free_itf_idx++;

    auto ep_out_idx = 0_u8;
    auto ep_in_idx  = 0_u8;
    if(strict_eps)
    {
        auto [ep_inout_idx, ep_inout] = find_unassigned_ep_inout(eps);
        ep_inout->out_itf_idx = msc_itf;
        ep_inout->out_itf     = itf_t::msc;
        ep_inout->out         = ept_t::msc_bulk_out;
        ep_inout->in_itf_idx  = msc_itf;
        ep_inout->in_itf      = itf_t::msc;
        ep_inout->in          = ept_t::msc_bulk_in;
        ep_in_idx             = ep_inout_idx;
        ep_out_idx            = ep_inout_idx;
    }
    else
    {
        auto [_ep_out_idx, ep_out] = find_unassigned_ep_out(eps);
        ep_out->out_itf_idx = msc_itf;
        ep_out->out_itf     = itf_t::msc;
        ep_out->out         = ept_t::msc_bulk_out;
        ep_out_idx          = _ep_out_idx;
        auto [_ep_in_idx, ep_in] = find_unassigned_ep_in(eps);
        ep_in->in_itf_idx = msc_itf;
        ep_in->in_itf     = itf_t::msc;
        ep_in->in         = ept_t::msc_bulk_in;
        ep_in_idx         = _ep_in_idx;
    }

    return state.append_desc({ TUD_MSC_DESCRIPTOR(
        msc_itf,
        string_descriptor::msc,
        ep_out_idx,
        (u8)(EP_DIR_IN | ep_in_idx),
        CFG_TUD_MSC_EP_BUFSIZE
    )});
}
