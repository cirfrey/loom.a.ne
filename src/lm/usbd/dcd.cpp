// Defaults for the externs.
#include "lm/usbd/dcd.hpp"

namespace lm::usbd::dcd
{
    [[gnu::weak]] dcd_vtable* phy_vtable = nullptr;
    [[gnu::weak]] dcd_vtable* usbip_vtable = nullptr;

    dcd_vtable* port_routing[max_ports] = {nullptr, nullptr};
    auto bind_port(u8 rhport, dcd_vtable* vtable) -> bool
    {
        if (rhport < max_ports) {
            port_routing[rhport] = vtable;
            return true;
        }
        return false;
    }
}

using namespace lm;
using namespace lm::usbd::dcd;

extern "C" {

#define DISPATCH(func, fallback, ...) \
    if (rhport < max_ports && port_routing[rhport] && port_routing[rhport]->func) { \
        return port_routing[rhport]->func(rhport, ##__VA_ARGS__); \
    } \
    return fallback;

#define DISPATCH_VOID(func, ...) \
    if (rhport < max_ports && port_routing[rhport] && port_routing[rhport]->func) { \
        port_routing[rhport]->func(rhport, ##__VA_ARGS__); \
    }

// Initialize controller to device mode
auto dcd_init(u8 rhport, const tusb_rhport_init_t* rh_init) -> bool { DISPATCH(dcd_init, false, rh_init); }
auto dcd_deinit(u8 rhport) -> bool { DISPATCH(dcd_deinit, false); }
auto dcd_int_handler(u8 rhport) -> void { DISPATCH_VOID(dcd_int_handler); }
// Enable device interrupt
auto dcd_int_enable(u8 rhport) -> void { DISPATCH_VOID(dcd_int_enable); }
// Disable device interrupt
auto dcd_int_disable(u8 rhport) -> void { DISPATCH_VOID(dcd_int_disable); }
// Receive Set Address request, mcu port must also include status IN response
auto dcd_set_address(u8 rhport, u8 dev_addr) -> void { DISPATCH_VOID(dcd_set_address, dev_addr); }
// Wake up host
auto dcd_remote_wakeup(u8 rhport) -> void { DISPATCH_VOID(dcd_remote_wakeup); }
// Connect by enabling internal pull-up resistor on D+/D-
auto dcd_connect(u8 rhport) -> void { DISPATCH_VOID(dcd_connect); }
// Disconnect by disabling internal pull-up resistor on D+/D-
auto dcd_disconnect(u8 rhport) -> void { DISPATCH_VOID(dcd_disconnect); }
auto dcd_sof_enable(u8 rhport, bool en) -> void { DISPATCH_VOID(dcd_sof_enable, en); }

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

auto dcd_edpt0_status_complete(u8 rhport, tusb_control_request_t const* request) -> void { DISPATCH_VOID(dcd_edpt0_status_complete, request); }

// Configure endpoint's registers according to descriptor
auto dcd_edpt_open(u8 rhport, tusb_desc_endpoint_t const* ep_desc) -> bool { DISPATCH(dcd_edpt_open, false, ep_desc); }
auto dcd_edpt_close_all (u8 rhport) -> void { DISPATCH_VOID(dcd_edpt_close_all); }
// Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
auto dcd_edpt_xfer(u8 rhport, u8 ep_addr, u8* buffer, u16 total_bytes) -> bool { DISPATCH(dcd_edpt_xfer, false, ep_addr, buffer, total_bytes); }
// Submit a transfer where is managed by FIFO, When complete dcd_event_xfer_complete() is invoked to notify the stack - optional, however, must be listed in usbd.c
auto dcd_edpt_xfer_fifo(u8 rhport, u8 ep_addr, tu_fifo_t* ff, u16 total_bytes) -> bool { DISPATCH(dcd_edpt_xfer_fifo, false, ep_addr, ff, total_bytes); }
// Stall endpoint
auto dcd_edpt_stall(u8 rhport, u8 ep_addr) -> void { DISPATCH_VOID(dcd_edpt_stall, ep_addr); }
// clear stall, data toggle is also reset to DATA0
auto dcd_edpt_clear_stall(u8 rhport, u8 ep_addr) -> void { DISPATCH_VOID(dcd_edpt_clear_stall, ep_addr); }

auto dcd_edpt_close(u8 rhport, u8 ep_addr) -> void { DISPATCH_VOID(dcd_edpt_close, ep_addr); }

// Allocate packet buffer used by ISO endpoints
// Some MCU need manual packet buffer allocation, we allocate the largest size to avoid clustering
auto dcd_edpt_iso_alloc(u8 rhport, u8 ep_addr, u16 largest_packet_size) -> bool { DISPATCH(dcd_edpt_iso_alloc, false, ep_addr, largest_packet_size); }
// Configure and enable an ISO endpoint according to descriptor
auto dcd_edpt_iso_activate(u8 rhport, tusb_desc_endpoint_t const * desc_ep) -> bool { DISPATCH(dcd_edpt_iso_activate, false, desc_ep); }
}
