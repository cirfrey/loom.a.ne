#include "lm/usbd/hid.hpp"

#include "tusb.h"

#if CFG_TUD_HID == 0

    #include "lm/log.hpp"
    auto lm::usbd::hid::do_configuration_descriptor(
        configuration_descriptor_builder_state_t& state,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void {
        cfg.hid = false;
        log::debug("HID disabled via CFG_TUD_HID=0\n");
    }

#else

namespace lm::usbd::hid
{
    static u8 const report_descriptor[] = {
        TUD_HID_REPORT_DESC_MOUSE( HID_REPORT_ID( (u8)hid_reportid::mouse) ),
        TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID( (u8)hid_reportid::keyboard) ),
        TUD_HID_REPORT_DESC_GAMEPAD( HID_REPORT_ID( (u8)hid_reportid::gamepad) ),

        // --- VENDOR-DEFINED LOGGING ---
        HID_USAGE_PAGE_N ( 0xFFAB, 2 ), // Vendor-defined page
        HID_USAGE_N      ( 0x0200, 2 ), // Vendor-defined usage
        HID_COLLECTION   ( HID_COLLECTION_APPLICATION ),
            HID_REPORT_ID  ( (u8)hid_reportid::vendor )
            HID_USAGE      ( 0x01 ),
            HID_LOGICAL_MIN( 0x00 ),
            HID_LOGICAL_MAX( 0xFF ),
            HID_REPORT_SIZE( 8    ), // 8 bits per byte
            HID_REPORT_COUNT( 63  ), // 63 bytes of text (plus 1 byte for ID = 64 total)
            HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        HID_COLLECTION_END
    };
}

auto lm::usbd::hid::do_configuration_descriptor(
    configuration_descriptor_builder_state_t& state,
    cfg_t& cfg,
    std::span<ep_t> eps
) -> void
{
    if (!cfg.hid) return;

    auto hid_itf = state.lowest_free_itf_idx++;

    auto [ep_in_idx, ep_in] = lm::usbd::find_unassigned_ep_in(eps);
    ep_in->in_itf_idx = hid_itf;
    ep_in->in_itf     = ep_t::interface_type_t::hid;
    ep_in->in         = ept_t::hid_interrupt_in;

    state.append_desc({ TUD_HID_DESCRIPTOR(
        hid_itf,
        (u8)string_descriptor_idxs::idx_hid,
        HID_ITF_PROTOCOL_NONE,
        sizeof(lm::usbd::hid::report_descriptor),
        (u8)(EP_DIR_IN | ep_in_idx), // epin.
        64, // epsize.
        1) // epinterval (poolin interval in ms).
    });
}

extern "C"
{
    uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf) {
        (void) itf;
        return lm::usbd::hid::report_descriptor; // Replace with your descriptor array name
    }

    uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
        (void) itf; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
        return 0;
    }

    void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
        (void) itf; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
    }

    bool tud_hid_set_idle_cb(uint8_t itf, uint8_t idle_rate) {
        (void) itf; (void) idle_rate;
        return true;
    }
}
#endif
