#include "lm/strands/usbd.hpp"

#include "lm/core/veil.hpp"

#include "lm/chip/usb.hpp"

#include "lm/config.hpp"
#include "lm/log.hpp"

#include "lm/usb/debug.hpp"
#include "lm/usb/backend.hpp"

#include <tusb.h>

namespace lm::strands
{
    static lm::strands::usbd* usbd_instance = nullptr;
}

#ifdef TUP_USBIP_DWC2
#include "portable/synopsys/dwc2/dwc2_common.h"
// Helper to get size in WORDS (Bytes / 4, rounded up)
constexpr lm::u16 get_ep_word_size(lm::usbd::ept_t type) {
    lm::u16 bytes = 0;
    switch (type) {
        case lm::usbd::ept_t::control:           bytes = 64; break;
        case lm::usbd::ept_t::cdc_bulk_in:       bytes = CFG_TUD_CDC_EP_BUFSIZE; break;
        case lm::usbd::ept_t::cdc_interrupt_in:  bytes = 16; break;
        case lm::usbd::ept_t::hid_interrupt_in:  bytes = CFG_TUD_HID_EP_BUFSIZE; break;
        case lm::usbd::ept_t::midi_bulk_in:      bytes = CFG_TUD_MIDI_EP_BUFSIZE; break;
        case lm::usbd::ept_t::msc_bulk_in:       bytes = CFG_TUD_MSC_EP_BUFSIZE; break;
        case lm::usbd::ept_t::uac2_iso_in:       bytes = CFG_TUD_AUDIO_EP_SZ_IN; break;
        case lm::usbd::ept_t::uac2_feedback:     bytes = 8; break;
        default: return 0;
    }
    return (bytes + 3) / 4;
}

// Returns the last sucessful EP.
auto apply_dynamic_fifo_allocation(std::span<lm::usbd::ep_t> eps) -> lm::st {
    using namespace lm;

    // Access the registers via the driver's own abstraction
    // On ESP32-S3, rhport is usually 0.
    dwc2_regs_t* dwc2 = DWC2_REG(0);

    u16 current_word_offset = 0;
    const u16 MAX_FIFO_WORDS = 256;

    // 1. Global RX FIFO
    // Shared by all OUT endpoints. 48 words is a safe starting point.
    u16 rx_words = 128;
    dwc2->grxfsiz = rx_words;
    current_word_offset += rx_words;

    for (size_t i = 0; i < eps.size(); ++i) {
        const auto& ep = eps[i];

        if (ep.in == usbd::ept_t::unassigned ||
            ep.in == usbd::ept_t::unavailable) {
            continue;
        }

        u16 ep_words = get_ep_word_size(ep.in);

        if (current_word_offset + ep_words > MAX_FIFO_WORDS) {
            return 0;
        }

        // 2. EP0 (Control IN) uses the Non-periodic TX FIFO register
        if (i == 0) {
            dwc2->gnptxfsiz = (ep_words << 16) | current_word_offset;
        }
        // 3. EP1-EP6 use the dedicated Device IN Endpoint TX FIFO array
        else {
            // dieptxf[0] maps to EP1, dieptxf[1] to EP2, etc.
            dwc2->dieptxf[i - 1] = (ep_words << 16) | current_word_offset;
        }

        current_word_offset += ep_words;
    }

    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::strand::sleep_ms(1);

    // 0x10 is the magic value for "Flush All TX FIFOs" in the DWC2 core.
    // This resets the pointers for both EP0 (gnptxfsiz) and EP1-6 (dieptxf).
    dfifo_flush_tx(dwc2, 0x10);
    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::strand::sleep_ms(1);

    // Resets the pointer for the Global RX FIFO.
    dfifo_flush_rx(dwc2);
    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::strand::sleep_ms(1);


    return current_word_offset;
}
#endif

lm::strands::usbd::usbd(fabric::strand_runtime_info& info)
{
    usbd_instance = this;

    get_status_q = fabric::queue<fabric::event>(1);
    get_status_q_tok = fabric::bus::subscribe(
        get_status_q,
        fabric::topic::usbd,
        { event::get_status }
    );

    auto lens = usb::backend::setup_descriptors(
        config_descriptor,
        string_descriptors,
        device_descriptor,
        config.usb,
        config.audio.backend.usb,
        config.cdc.backend.usb,
        config.hid.backend.usb,
        config.midi.backend.usb,
        config.msc.backend.usb
    );
    log::debug<128 * 3>(
        "Config descriptor len && endpoint map report.\n"
        "\t+--------+-----+-----+------+-----+-------+-------+\n"
        "\t| Header | CDC | HID | MIDI | MSC | AUDIO | Total |\n"
        "\t+--------+-----+-----+------+-----+-------+-------+\n"
        "\t| %-6u | %-3u | %-3u | %-4u | %-3u | %-5u | %-5u |\n"
        "\t+--------+-----+-----+------+-----+-------+-------+\n",
        lens.header, lens.cdc, lens.hid, lens.midi, lens.msc, lens.audio, lens.total
    );
    auto printer = [](auto fmt, auto... args){ log::debug<128>(
        log::fmt_t({ .fmt = fmt, .timestamp = log::no_timestamp, .filename = log::no_filename }),
        veil::forward<decltype(args)>(args)...
    ); };
    usb::debug::print_ep_table(config.usb.endpoints, printer, {"\t", 1});

    chip::usb::phy::power_up();
}

auto lm::strands::usbd::on_ready() -> fabric::managed_strand_status
{
    tud_strand_handle = fabric::strand::create(_config::strand::tud, {}, [](void* p){
        auto self = (usbd*)p;
        tusb_init();
        #ifdef TUP_USBIP_DWC2
            auto eps = apply_dynamic_fifo_allocation(self->endpoints);
            log::warn("apply_dynamic_fifo_allocation allocated %zu words\n", eps);
        #endif
        while(1) tud_task();
    }, this);

    broadcast_status();

    return fabric::managed_strand_status::ok;
}

auto lm::strands::usbd::before_sleep() -> fabric::managed_strand_status
{ return fabric::managed_strand_status::ok; }

auto lm::strands::usbd::on_wake() -> fabric::managed_strand_status
{
    for(auto& e : get_status_q.consume<fabric::event>())
    { broadcast_status(); }

    return fabric::managed_strand_status::ok;
}

lm::strands::usbd::~usbd() {
    if(tud_strand_handle) {
        fabric::strand::reap(tud_strand_handle);
        tusb_deinit(0);
    }
}

auto lm::strands::usbd::broadcast_status() -> void
{
    // TODO:
    // fabric::bus::publish({
    //     .topic = fabric::topic::usbd,
    //     .type  = cfg.cdc ? event::cdc_enabled : event::cdc_disabled
    // });
    // fabric::bus::publish({
    //     .topic = fabric::topic::usbd,
    //     .type  = cfg.hid ? event::hid_enabled : event::hid_disabled
    // });
}

extern "C" {

// Device Descriptor.
auto tud_descriptor_device_cb() -> lm::u8 const*
{
    if(!lm::strands::usbd_instance) return nullptr;
    return (lm::u8 const*) &lm::strands::usbd_instance->device_descriptor;
}

// Configuration Descriptor.
auto tud_descriptor_configuration_cb(uint8_t index) -> lm::u8 const*
{
    if(!lm::strands::usbd_instance) return nullptr;
    return lm::strands::usbd_instance->config_descriptor;
}

// String Descriptors.
auto tud_descriptor_string_cb(lm::u8 index, lm::u16 langid) -> lm::u16 const*
{
    using namespace lm;

    if(!strands::usbd_instance) return nullptr;

    // Shared buffer for string descriptors
    static u16 _desc_str[config_t::usbcommon::string_descriptor_max_len];

    // --- Index 0 is NOT a string, it's the Language ID list ---
    if (index == (u8)usb::string_descriptor::lang)
    {
        static u16 const lang_id[] = { (u16)((TUSB_DESC_STRING << 8) | 0x04), 0x0409 };
        return lang_id;
    }

    // Helper to convert UTF-8 (char*) to UTF-16 (uint16_t)
    auto utf8_to_utf16 = [](const char* str) -> u16* {
        if (!str) str = "";

        u16 chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        // First byte is Length (Total bytes including header), Second is Type (0x03)
        // In TinyUSB, we return this as a uint16_t array where the first element
        // packs these two bytes: (0x03 << 8) | (Length)
        _desc_str[0] = (uint16_t)((0x03 << 8) | (2 * chr_count + 2));

        // Convert ASCII/UTF8 chars to UTF16 (simple cast for standard ASCII)
        for (uint8_t i = 0; i < chr_count; i++) { _desc_str[1 + i] = str[i]; }

        return _desc_str;
    };
    // Return the UTF-16 string for the given index
    return utf8_to_utf16( strands::usbd_instance->string_descriptors[index] );
}

void tud_mount_cb(void) {}
void tud_umount_cb(void) {}
void tud_suspend_cb(bool remote_wakeup_en) {}
void tud_resume_cb(void) {}

} // extern "C"
