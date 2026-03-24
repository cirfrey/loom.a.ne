#include "espy/usb/usb.hpp"
#include "espy/usb/esp32.hpp"
#include "espy/usb/logging.hpp"
#include "espy/blink.hpp"

#include "freertos/FreeRTOS.h"

#include "tusb.h"

static espy::blink_task_params blink_params = {
    .bc = espy::blink,
    .gpio_pin = GPIO_NUM_15
};

using cfg = espy::usb::configuration_t;
cfg cdc_midi_hid = { // 6 EPs.
    .cdc = true,
    .uac = cfg::no_uac,
    .hid = true,
    .midi = cfg::midi_inout,
};
cfg cdc_uac_hid = { // 6 EPs.
    .cdc = true,
    .uac = cfg::speaker,
    .speaker_channels = 2,
    .hid = true,
    .midi = cfg::no_midi,
};
cfg cfg_full = { // 6 EPs.
    .cdc = false,
    .uac = cfg::speaker_microphone,
    .speaker_channels = 2,
    .microphone_channels = 1,
    .hid = true,
    .midi = cfg::midi_inout,
};

#include "espy/renum.hpp"

void print_usb_topology(std::span<espy::usb::endpoint_info_t> eps) {
    using namespace espy::usb;

    // Reflect the enums
    // Adjust Lo/Hi range if your enum values exceed -128 to 128
    auto type_renum = espy::renum::reflect<endpoint_info_t::type_t, 0, 20>{};
    auto itf_renum  = espy::renum::reflect<endpoint_info_t::interface_type_t, 0, 10>{};
    auto dir_renum  = espy::renum::reflect<endpoint_info_t::direction_t, 0, 10>{};

    const char* separator = "+----+-------------------+-----+------------------+-----------+";

    logging::logf("\r\nUSB Endpoint Topology Map:\r\n");
    logging::logf("%s\r\n", separator);
    logging::logf("| EP | Type              | Itf | Interface Class  | Direction |\r\n");
    logging::logf("%s\r\n", separator);

    for (size_t i = 0; i < eps.size(); ++i) {
        const auto& ep = eps[i];

        // Get string views from renum
        auto type_v = std::string{ type_renum[ep.type].unqualified() };
        auto itf_v  = std::string{ itf_renum[ep.interface_type].unqualified() };
        auto dir_v  = std::string{ dir_renum[ep.configured_direction].unqualified() };

        // Print row using %.*s for non-null-terminated string_views
        logging::logf("| %-2d | %-17s | %-3d | %-16s | %-9s |\r\n",
            (int)i,
            (int)type_v.c_str(),
            (int)ep.interface,
            (int)itf_v.c_str(),
            (int)dir_v.c_str()
        );
    }

    logging::logf("%s\r\n\r\n", separator);
}

void send_midi_note_manual(uint8_t cable, uint8_t channel, uint8_t note, uint8_t velocity) {
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

extern "C" auto app_main() -> void
{
    auto blink_task_handle = espy::create_blink_task(&blink_params);

    cfg cfg = {
        .cdc = false,
        .uac = cfg::no_uac,
        .hid = true,
        .midi = cfg::no_midi,
        .midi_cable_count = 4,
    };
    espy::usb::init(cdc_midi_hid, espy::usb::esp32_endpoints);
    //espy::usb::cdc::init();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        print_usb_topology(espy::usb::esp32_endpoints);
        // send_midi_note_manual(cable, 0, 62, 127);
    }
    vTaskDelete(blink_task_handle);
}
