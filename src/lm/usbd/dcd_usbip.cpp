#include "lm/usbd/dcd.hpp"

namespace lm::usbd::dcd::usbip
{
    /*------------------------------------------------------------------*/
    /* Device API
    *------------------------------------------------------------------*/

    // Initialize controller to device mode
    bool dcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
    (void) rhport; (void) rh_init;
    return true;
    }

    // Enable device interrupt
    void dcd_int_enable (uint8_t rhport) {
    (void) rhport;
    }

    // Disable device interrupt
    void dcd_int_disable (uint8_t rhport) {
    (void) rhport;
    }

    // Receive Set Address request, mcu port must also include status IN response
    void dcd_set_address (uint8_t rhport, uint8_t dev_addr) {
    (void) rhport;
    (void) dev_addr;
    }

    // Wake up host
    void dcd_remote_wakeup (uint8_t rhport) {
    (void) rhport;
    }

    // Connect by enabling internal pull-up resistor on D+/D-
    void dcd_connect(uint8_t rhport) {
    (void) rhport;
    }

    // Disconnect by disabling internal pull-up resistor on D+/D-
    void dcd_disconnect(uint8_t rhport) {
    (void) rhport;
    }

    void dcd_sof_enable(uint8_t rhport, bool en) {
    (void) rhport;
    (void) en;
    }

    //--------------------------------------------------------------------+
    // Endpoint API
    //--------------------------------------------------------------------+

    // Configure endpoint's registers according to descriptor
    bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * ep_desc) {
    (void) rhport;
    (void) ep_desc;
    return false;
    }

    // Allocate packet buffer used by ISO endpoints
    // Some MCU need manual packet buffer allocation, we allocate the largest size to avoid clustering
    bool dcd_edpt_iso_alloc(uint8_t rhport, uint8_t ep_addr, uint16_t largest_packet_size) {
    (void) rhport;
    (void) ep_addr;
    (void) largest_packet_size;
    return false;
    }

    // Configure and enable an ISO endpoint according to descriptor
    bool dcd_edpt_iso_activate(uint8_t rhport, tusb_desc_endpoint_t const * desc_ep) {
    (void) rhport;
    (void) desc_ep;
    return false;
    }

    void dcd_edpt_close_all (uint8_t rhport) {
    (void) rhport;
    }

    // Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
    bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes) {
    (void) rhport;
    (void) ep_addr;
    (void) buffer;
    (void) total_bytes;
    return false;
    }

    // Submit a transfer where is managed by FIFO, When complete dcd_event_xfer_complete() is invoked to notify the stack - optional, however, must be listed in usbd.c
    bool dcd_edpt_xfer_fifo (uint8_t rhport, uint8_t ep_addr, tu_fifo_t * ff, uint16_t total_bytes) {
    (void) rhport;
    (void) ep_addr;
    (void) ff;
    (void) total_bytes;
    return false;
    }

    // Stall endpoint
    void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr) {
    (void) rhport;
    (void) ep_addr;
    }

    // clear stall, data toggle is also reset to DATA0
    void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr) {
    (void) rhport;
    (void) ep_addr;
    }

    static auto vtable = dcd_vtable {
        .dcd_init = dcd_init,
        .dcd_deinit = nullptr,
        .dcd_int_handler = nullptr,
        .dcd_int_enable = dcd_int_enable,
        .dcd_int_disable = dcd_int_disable,
        .dcd_set_address = dcd_set_address,
        .dcd_remote_wakeup = dcd_remote_wakeup,
        .dcd_connect = dcd_connect,
        .dcd_disconnect = dcd_disconnect,
        .dcd_sof_enable = dcd_sof_enable,
        .dcd_edpt0_status_complete = nullptr,
        .dcd_edpt_open = dcd_edpt_open,
        .dcd_edpt_close_all = dcd_edpt_close_all,
        .dcd_edpt_xfer = dcd_edpt_xfer,
        .dcd_edpt_xfer_fifo = dcd_edpt_xfer_fifo,
        .dcd_edpt_stall = dcd_edpt_stall,
        .dcd_edpt_clear_stall = dcd_edpt_clear_stall,
        .dcd_edpt_close = nullptr,
        .dcd_edpt_iso_alloc = dcd_edpt_iso_alloc,
        .dcd_edpt_iso_activate = dcd_edpt_iso_activate
    };
}

namespace lm::usbd::dcd
{
    dcd_vtable* usbip_vtable = &usbip::vtable;
}
