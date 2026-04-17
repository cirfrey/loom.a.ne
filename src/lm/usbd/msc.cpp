#include "lm/usbd/msc.hpp"

#include <tusb.h>

#include "lm/config.hpp"
#include "lm/log.hpp"

#include "lm/chip/memory.hpp"

static lm::chip::memory::storage* static_storage = nullptr;
void init_msc_partition() {
    auto storages = lm::chip::memory::get_storages();

    if (storages.empty()) {
        lm::log::error("No storages found! Check flash/HAL settings.\n");
        return;
    }

    lm::log::debug("Listing all storages...\n");
    for(auto& s : storages)
    {
        lm::log::debug(
            "Found: [%.*s] Type: 0x%02x Subtype: 0x%02x Size: %zu bytes\n",
            (int)s.label.size, s.label.data,
            s.type, s.subtype, s.size
        );

        if (s.label == "static") {
            static_storage = &s;
        }
    }
}

auto lm::usbd::msc::do_configuration_descriptor(
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

    // TODO: move me to lm::hook::arch_config.
    init_msc_partition();
    lm::log::info("Initialized MSC partition with %p\n", static_storage);
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
    return (static_storage != nullptr);
}

// Mandatory: Capacity
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {
    (void) lun;
    if (static_storage) {
        // ESP32 flash sectors are typically 4096 bytes,
        // but USB MSC usually expects 512-byte logical blocks.
        *block_size  = static_storage->sector_size;
        *block_count = static_storage->size / (*block_size);
    } else {
        *block_count = 0;
        *block_size  = 0;
    }
}

// Mandatory: Read10
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun;
    if (static_storage == nullptr) return -1;

    // Calculate the flash address: (LBA * block_size) + offset
    // TinyUSB usually gives offset=0 and bufsize as a multiple of block_size
    uint32_t addr = (lba * static_storage->sector_size) + offset;

    auto err = static_storage->read(addr, {buffer, bufsize});
    return (err == lm::chip::memory::storage_op_status::ok) ? (int32_t)bufsize : -1;
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
