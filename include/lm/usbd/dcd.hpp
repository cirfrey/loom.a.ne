#pragma once

#include "lm/core/types.hpp"

#include <tusb.h>

namespace lm::usbd::dcd {
    struct dcd_vtable;

    // The DCD functions for the phy usb on whatever backend is compiled.
    // E.g: DWC2 on ESP32s2, nothing on native.
    // They are defaulted to nullptr if not compiled.
    extern dcd_vtable* phy_vtable;
    extern dcd_vtable* usbip_vtable;

    constexpr uint8_t max_ports = 2;
    auto bind_port(uint8_t rhport, dcd_vtable* vtable) -> bool;
}

struct lm::usbd::dcd::dcd_vtable
{
    // Initialize controller to device mode
    bool (*dcd_init)(u8 rhport, const tusb_rhport_init_t* rh_init);
    // Deinitialize controller, unset device mode.
    bool (*dcd_deinit)(u8 rhport);
    // Interrupt Handler
    void (*dcd_int_handler)(u8 rhport);
    // Enable device interrupt
    void (*dcd_int_enable)(u8 rhport);
    // Disable device interrupt
    void (*dcd_int_disable)(u8 rhport);
    // Receive Set Address request, mcu port must also include status IN response
    void (*dcd_set_address)(u8 rhport, u8 dev_addr);
    // Wake up host
    void (*dcd_remote_wakeup)(u8 rhport);
    // Connect by enabling internal pull-up resistor on D+/D-
    void (*dcd_connect)(u8 rhport);
    // Disconnect by disabling internal pull-up resistor on D+/D-
    void (*dcd_disconnect)(u8 rhport);
    // Enable/Disable Start-of-frame interrupt. Default is disabled
    void (*dcd_sof_enable)(u8 rhport, bool en);

    // Invoked when a control transfer's status stage is complete.
    // May help DCD to prepare for next control transfer, this API is optional.
    void (*dcd_edpt0_status_complete)(u8 rhport, tusb_control_request_t const * request);
    // Configure endpoint's registers according to descriptor
    bool (*dcd_edpt_open)            (u8 rhport, tusb_desc_endpoint_t const * desc_ep);
    // Close all non-control endpoints, cancel all pending transfers if any.
    // Invoked when switching from a non-zero Configuration by SET_CONFIGURE therefore
    // required for multiple configuration support.
    void (*dcd_edpt_close_all)       (u8 rhport);
    // Submit a transfer, When complete dcd_event_xfer_complete() is invoked to notify the stack
    bool (*dcd_edpt_xfer)            (u8 rhport, u8 ep_addr, u8* buffer, u16 total_bytes);
    // Submit an transfer using fifo, When complete dcd_event_xfer_complete() is invoked to notify the stack
    // This API is optional, may be useful for register-based for transferring data.
    bool (*dcd_edpt_xfer_fifo)       (u8 rhport, u8 ep_addr, tu_fifo_t* ff, u16 total_bytes);
    // Stall endpoint, any queuing transfer should be removed from endpoint
    void (*dcd_edpt_stall)           (u8 rhport, u8 ep_addr);
    // clear stall, data toggle is also reset to DATA0
    // This API never calls with control endpoints, since it is auto cleared when receiving setup packet
    void (*dcd_edpt_clear_stall)     (u8 rhport, u8 ep_addr);

    // Close an endpoint.
    void (*dcd_edpt_close)(u8 rhport, u8 ep_addr);

    // Allocate packet buffer used by ISO endpoints
    // Some MCU need manual packet buffer allocation, we allocate the largest size to avoid clustering
    bool (*dcd_edpt_iso_alloc)(u8 rhport, u8 ep_addr, u16 largest_packet_size);
    // Configure and enable an ISO endpoint according to descriptor
    bool (*dcd_edpt_iso_activate)(u8 rhport, tusb_desc_endpoint_t const* desc_ep);
};
