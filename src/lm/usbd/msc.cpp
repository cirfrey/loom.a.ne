#include "lm/usbd/msc.hpp"

#include "lm/config.hpp"

#include <esp_partition.h>
// #include <sdmmc_cmd.h> // Later - for SD card.

#include <tusb.h>

auto lm::usbd::msc::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    if(!cfg.msc) return;

    auto msc_itf = state.lowest_free_itf_idx++;

    /// --- Securing endpoints ---
    // NOTE: EP IN and EP OUT share interfaces
    auto [ep_out_idx, ep_out]    = lm::usbd::find_unassigned_ep(eps, dir_t::OUT, dir_t::INOUT);
    ep_out->configured_direction = ep_t::direction_t::OUT;
    ep_out->interface_type       = ep_t::interface_type_t::msc;
    ep_out->interface            = msc_itf;
    ep_out->type                 = ept_t::msc_bulk_out;

    auto [ep_in_idx, ep_in]     = lm::usbd::find_unassigned_ep(eps, dir_t::IN, dir_t::INOUT);
    ep_in->configured_direction = ep_t::direction_t::IN;
    ep_in->interface_type       = ep_t::interface_type_t::msc;
    ep_in->interface            = msc_itf;
    ep_in->type                 = ept_t::msc_bulk_in;

    state.append_desc({ TUD_MSC_DESCRIPTOR(
        msc_itf,
        (u8)string_descriptor_idxs::idx_msc,
        ep_out_idx,
        (u8)(EP_DIR_IN | ep_in_idx),
        CFG_TUD_MSC_EP_BUFSIZE
    )});
}

extern "C" {

// Mandatory: Max LUN (0-based)
uint8_t tud_msc_get_max_lun_cb(void) {
    return 0; // Only one drive
}

// Mandatory: Inquiry (8, 16, 4 bytes)
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    (void) lun;
    // IMPORTANT: Note the explicit lengths and padding
    memcpy(vendor_id,   "LoomANe ", 8);
    memcpy(product_id,  "Static-Flash    ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

// Mandatory: Test Unit Ready
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void) lun;
    return true; // Must be true for the disk to "appear"
}

// Mandatory: Capacity
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void) lun;
    *block_size = 512;
    *block_count = 2048; // 1MB for testing
}

// Mandatory: Read10
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun; (void) lba; (void) offset;
    memset(buffer, 0, bufsize);
    return (int32_t) bufsize;
}

// Mandatory: Write10
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void) lun; (void) lba; (void) offset; (void) buffer;
    return (int32_t) bufsize;
}

// Mandatory: SCSI Start/Stop/Custom
// NOTE: scsi_cmd is a pointer to the 16-byte buffer
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    return -1; // -1 means "let TinyUSB handle standard SCSI commands"
}

} // extern "C"
