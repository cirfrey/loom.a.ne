#include "lm/usbd/msc.hpp"

#include "lm/config.hpp"
#include "lm/log.hpp"

/// TODO: abstract.
#include <esp_partition.h>
// #include <sdmmc_cmd.h> // Later - for SD card.

#include <tusb.h>

static const esp_partition_t* s_static_partition = nullptr;
void init_msc_partition() {
    lm::log::debug("Listing all partitions...\n");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);

    bool found_any = false;
    while (it != NULL) {
        const esp_partition_t* p = esp_partition_get(it);
        lm::log::debug("Found: [%s] Type: 0x%02x Subtype: 0x%02x Size: %lu\n",
                 p->label, p->type, p->subtype, p->size);
        it = esp_partition_next(it);
        found_any = true;
    }
    esp_partition_iterator_release(it);

    if (!found_any) {
        lm::log::debug("No partition table found at all! Check your flash settings.\n");
    }

    // Now try the specific find again
    s_static_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "static");
}

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

    init_msc_partition();
    lm::log::info("Initialized MSC partition with %p\n", s_static_partition);
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
    return (s_static_partition != nullptr);
}

// Mandatory: Capacity
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void) lun;
    if (s_static_partition) {
        // ESP32 flash sectors are typically 4096 bytes,
        // but USB MSC usually expects 512-byte logical blocks.
        *block_size = 4096;
        *block_count = s_static_partition->size / (*block_size);
    } else {
        *block_count = 0;
        *block_size = 0;
    }
}

// Mandatory: Read10
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun;
    if (!s_static_partition) return -1;

    // Calculate the flash address: (LBA * block_size) + offset
    // TinyUSB usually gives offset=0 and bufsize as a multiple of block_size
    uint32_t addr = (lba * 512) + offset;

    esp_err_t err = esp_partition_read(s_static_partition, addr, buffer, bufsize);
    return (err == ESP_OK) ? (int32_t)bufsize : -1;
}

// Optional but recommended: Mark as Write Protected
bool tud_msc_is_writable_cb(uint8_t lun) {
    (void) lun;
    return false; // Tells the host OS the drive is Read-Only
}

// Mandatory: Write10
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void) lun; (void) lba; (void) offset; (void) buffer;
    return -1;
}

// Mandatory: SCSI Start/Stop/Custom
// NOTE: scsi_cmd is a pointer to the 16-byte buffer
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize) {
    (void) lun; (void) scsi_cmd; (void) buffer; (void) bufsize;
    return -1; // -1 means "let TinyUSB handle standard SCSI commands"
}

} // extern "C"
