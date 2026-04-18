#pragma once

#include "lm/core/types.hpp"

#include "tusb.h"

namespace lm::hid
{
    namespace reportid { enum reportid_t : u8 {
        mouse    = 1,
        keyboard = 2,
        gamepad  = 3, // Standard 16-button, 2-joystick, 8-way hat gamepad.
        vendor   = 4  // See configuration_t::logging_t::hid_vendor_logging.
    }; }

    constexpr u8 report_descriptor[] = {
        TUD_HID_REPORT_DESC_MOUSE(    HID_REPORT_ID( reportid::mouse) ),
        TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID( reportid::keyboard) ),
        TUD_HID_REPORT_DESC_GAMEPAD(  HID_REPORT_ID( reportid::gamepad) ),

        // --- VENDOR-DEFINED LOGGING ---
        HID_USAGE_PAGE_N ( 0xFFAB, 2 ), // Vendor-defined page
        HID_USAGE_N      ( 0x0200, 2 ), // Vendor-defined usage
        HID_COLLECTION   ( HID_COLLECTION_APPLICATION ),
            HID_REPORT_ID  ( reportid::vendor )
            HID_USAGE      ( 0x01 ),
            HID_LOGICAL_MIN( 0x00 ),
            HID_LOGICAL_MAX( 0xFF ),
            HID_REPORT_SIZE( 8    ), // 8 bits per byte
            HID_REPORT_COUNT( 63  ), // 63 bytes of text (plus 1 byte for ID = 64 total)
            HID_INPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),
        HID_COLLECTION_END
    };
}
