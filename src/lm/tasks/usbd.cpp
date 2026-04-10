#include "lm/tasks/usbd.hpp"

#include "lm/chip/usb.hpp"
#include "lm/usbd/esp32.hpp" // TODO: abstract

#include "lm/config.hpp"
#include "lm/log.hpp"

#include <tusb.h>

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

    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::task::sleep_ms(1);

    // 0x10 is the magic value for "Flush All TX FIFOs" in the DWC2 core.
    // This resets the pointers for both EP0 (gnptxfsiz) and EP1-6 (dieptxf).
    dfifo_flush_tx(dwc2, 0x10);
    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::task::sleep_ms(1);

    // Resets the pointer for the Global RX FIFO.
    dfifo_flush_rx(dwc2);
    while (!(dwc2->grstctl & GRSTCTL_AHBIDL)) fabric::task::sleep_ms(1);


    return current_word_offset;
}
#endif

lm::tasks::usbd::usbd(fabric::task_runtime_info& info)
{
    get_status_q = fabric::queue<fabric::event>(1);
    get_status_q_tok = fabric::bus::subscribe(
        get_status_q,
        fabric::topic::usbd,
        { event::get_status }
    );

    cfg = {
        .cdc  = false,
        .uac  = lm::usbd::cfg_t::no_uac,
        .hid  = true,
        .midi = lm::usbd::cfg_t::midi_inout,
        .midi_cable_count = 1,
        .msc  = true,
    };
    endpoints = lm::usbd::esp32_endpoints;

    /// TODO: refactor all usbd internal state into this task.
    lm::usbd::init(cfg, endpoints, { .product_id=0x2015 });
    chip::usb::phy::power_up();

    constexpr auto ep_count = 7;
    char ept_buf[lm::usbd::debug::ep_table_size<ep_count> + (ep_count + 4 ) * sizeof("\t" - 1)];
    auto ept = lm::usbd::debug::eps_to_table(endpoints, {ept_buf, sizeof(ept_buf)}, {"\t", 1});
    log::debug("Final ep table \n");
    log::dispatch({ept.data, ept.size});
}

auto lm::tasks::usbd::on_ready() -> fabric::managed_task_status
{
    tud_task_handle = fabric::task::create(config::task::tud, {}, [](void* p){
        auto self = (usbd*)p;
        tusb_init();
        #ifdef TUP_USBIP_DWC2
            auto eps = apply_dynamic_fifo_allocation(self->endpoints);
            log::warn("apply_dynamic_fifo_allocation allocated %zu words\n", eps);
        #endif
        while(1) tud_task();
    }, this);

    broadcast_status();

    return fabric::managed_task_status::ok;
}

auto lm::tasks::usbd::before_sleep() -> fabric::managed_task_status
{ return fabric::managed_task_status::ok; }

auto lm::tasks::usbd::on_wake() -> fabric::managed_task_status
{
    for(auto& e : get_status_q.consume<fabric::event>())
    { broadcast_status(); }

    return fabric::managed_task_status::ok;
}

lm::tasks::usbd::~usbd() {
    if(tud_task_handle) {
        fabric::task::reap(tud_task_handle);
        tusb_deinit(0);
    }
}

auto lm::tasks::usbd::broadcast_status() -> void
{
    fabric::bus::publish({
        .topic = fabric::topic::usbd,
        .type  = cfg.cdc ? event::cdc_enabled : event::cdc_disabled
    });
    fabric::bus::publish({
        .topic = fabric::topic::usbd,
        .type  = cfg.hid ? event::hid_enabled : event::hid_disabled
    });
}
