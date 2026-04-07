#include "lm/usbd/cdc.hpp"

#include "tusb.h"

#if CFG_TUD_CDC == 0

    #include "lm/log.hpp"
    auto lm::usbd::cdc::do_configuration_descriptor(
        configuration_descriptor_builder_state_t& state,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void {
        cfg.cdc = false;
        log::debug("CDC disabled via CFG_TUD_CDC=0\n");
    }

#else

auto lm::usbd::cdc::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    if (!cfg.cdc) return;

    auto cdc_comm_itf = state.lowest_free_itf_idx++;
    auto cdc_data_itf = state.lowest_free_itf_idx++;

    auto [ep_notif_idx, ep_notif] = lm::usbd::find_unassigned_ep_in(eps);
    ep_notif->in_itf_idx = cdc_comm_itf;
    ep_notif->in_itf     = itf_t::cdc_comm;
    ep_notif->in         = ept_t::cdc_interrupt_in;

    auto ep_out_idx = 0_u8;
    auto ep_in_idx  = 0_u8;
    if(cfg.midi_strict_endpoints)
    {
        auto [ep_inout_idx, ep_inout] = lm::usbd::find_unassigned_ep_inout(eps);
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
        auto [_ep_out_idx, ep_out] = lm::usbd::find_unassigned_ep_out(eps);
        ep_out->out_itf_idx = cdc_data_itf;
        ep_out->out_itf     = itf_t::midi_streaming;
        ep_out->out         = ept_t::cdc_bulk_out;
        ep_out_idx          = _ep_out_idx;
        auto [_ep_in_idx, ep_in] = lm::usbd::find_unassigned_ep_in(eps);
        ep_in->in_itf_idx = cdc_data_itf;
        ep_in->in_itf     = itf_t::midi_streaming;
        ep_in->in         = ept_t::cdc_bulk_in;
        ep_in_idx         = _ep_in_idx;
    }

    state.append_desc({ TUD_CDC_DESCRIPTOR(
        cdc_comm_itf, // first itfnum (next one is itfnum+1).
        (u8)string_descriptor_idxs::idx_cdc, // stridx.
        (u8)(EP_DIR_IN | ep_notif_idx), // ep_notif.
        8, // ep_notif_size.
        ep_out_idx, // epout.
        (u8)(EP_DIR_IN | ep_in_idx), // epin.
        CFG_TUD_CDC_EP_BUFSIZE // ep_size.
    )});

}

extern "C"
{
    void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
        (void) itf; (void) dtr; (void) rts;
    }
    void tud_cdc_rx_cb(uint8_t itf) { (void) itf; }
}

#endif
