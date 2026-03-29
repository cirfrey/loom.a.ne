#include "lm/usbd/cdc.hpp"

#include "tusb.h"

auto lm::usbd::cdc::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    if (!cfg.cdc) return;

    auto cdc_comm_itf = state.lowest_free_itf_idx++;
    auto cdc_data_itf = state.lowest_free_itf_idx++;

    auto [ep_notif_idx, ep_notif]  = lm::usbd::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
    ep_notif->configured_direction = dir_t::IN;
    ep_notif->interface_type       = itf_t::cdc_comm;
    ep_notif->interface            = cdc_comm_itf;
    ep_notif->type                 = ept_t::cdc_interrupt_in;

    // NOTE: EP IN and EP OUT share interfaces.
    auto [ep_out_idx, ep_out]    = lm::usbd::find_unassigned_ep(eps, dir_t::OUT, dir_t::INOUT);
    ep_out->configured_direction = dir_t::OUT;
    ep_out->interface_type       = itf_t::cdc_data;
    ep_out->interface            = cdc_data_itf;
    ep_out->type                 = ept_t::cdc_bulk_out;

    auto [ep_in_idx, ep_in]     = lm::usbd::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
    ep_in->configured_direction = dir_t::IN;
    ep_in->interface_type       = itf_t::cdc_data;
    ep_in->interface            = cdc_data_itf;
    ep_in->type                 = ept_t::cdc_bulk_in;

    state.append_desc({ TUD_CDC_DESCRIPTOR(
        cdc_comm_itf, // first itfnum (next one is itfnum+1).
        (u8)string_descriptor_idxs::idx_cdc, // stridx.
        (u8)(EP_DIR_IN | ep_notif_idx), // ep_notif.
        8, // ep_notif_size.
        ep_out_idx, // epout.
        (u8)(EP_DIR_IN | ep_in_idx), // epin.
        64 // ep_size.
    )});
}

extern "C"
{
    void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
        (void) itf; (void) dtr; (void) rts;
    }
    void tud_cdc_rx_cb(uint8_t itf) { (void) itf; }
}
