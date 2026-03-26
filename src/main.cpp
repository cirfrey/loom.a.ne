
#include "espy/logging.hpp"
#include "espy/config.hpp"
#include "espy/blink.hpp"
#include "espy/task.hpp"
#include "espy/bus.hpp"

#include "espy/usb/usb.hpp"
#include "espy/usb/esp32.hpp"

#include "freertos/FreeRTOS.h"


#include "tusb.h"

/// TODO: move me to espy::debug.
#include "espy/renum.hpp"
void print_usb_topology(std::span<espy::usb::endpoint_info_t> eps) {
    using namespace espy;
    using namespace espy::usb;

    // Reflect the enums
    // Adjust Lo/Hi range if your enum values exceed -128 to 128
    auto type_renum = espy::renum::reflect<endpoint_info_t::type_t, 0, 20>{};
    auto itf_renum  = espy::renum::reflect<endpoint_info_t::interface_type_t, 0, 10>{};
    auto dir_renum  = espy::renum::reflect<endpoint_info_t::direction_t, 0, 10>{};

    const char* separator = "+----+-------------------+-----+------------------+-----------+----------------------+";

    logging::logf("USB Endpoint Topology Map:\r\n");
    logging::logf("%s\r\n", separator);
    logging::logf("| EP | Type              | Itf | Interface Class  | Direction | Available Directions |\r\n");
    logging::logf("%s\r\n", separator);

    for (size_t i = 0; i < eps.size(); ++i) {
        const auto& ep = eps[i];

        // Get string views from renum
        auto type_v      = type_renum[ep.type].unqualified();
        auto itf_v       = itf_renum[ep.interface_type].unqualified();
        auto dir_v       = dir_renum[ep.configured_direction].unqualified();
        auto avail_dir_v = dir_renum[ep.available_directions].unqualified();

        // Print row using %.*s for non-null-terminated string_views
        logging::logf("| %-2d | %-17.*s | %-3d | %-16.*s | %-9.*s | %-20.*s |\r\n",
            (int)i,
            (int)type_v.length(), type_v.data(),
            (int)ep.interface,
            (int)itf_v.length(), itf_v.data(),
            (int)dir_v.length(), dir_v.data(),
            (int)avail_dir_v.length(), avail_dir_v.data()
        );
    }

    logging::logf("%s\r\n\r\n", separator);
}

extern "C" auto app_main() -> void
{
    espy::bus::init();

    // Just to make sure we don't crash by writing to a null ringbuf.
    // If we log before setting up the usb we won't see anything in the CDC or HID (maybe espnow once that is implemented).
    espy::logging::init();

    espy::task::create(espy::config::task::blink, espy::blink::task);

    espy::usb::cfg_t cfg = {
        .cdc = false,
        .uac = espy::usb::cfg_t::microphone,
        .hid = true,
        .midi = espy::usb::cfg_t::midi_inout,
        .midi_cable_count = 8,
        .msc = false,
    };
    espy::usb::init(cfg, espy::usb::esp32_endpoints, { .product_id = 0x00001 });

    auto loopid = 0;
    while (1)
    {
        espy::task::delay_ms(1000);
        espy::logging::logf("--- Loop %i:\n", loopid++);
        print_usb_topology(espy::usb::esp32_endpoints);
        // send_midi_note_manual(cable, 0, 62, 127);
    }
}
