#pragma once

// USB/IP device-side protocol structs and constants.
//
// Wire format reference: https://docs.kernel.org/usb/usbip_protocol.html
// Protocol version: v1.1.1 (binary: 0x0111)
//
// ALL multi-byte fields are big-endian on the wire.
// The caller is responsible for hton/ntoh before filling and after reading.

#include "lm/core/types.hpp"

namespace lm::usbip
{
    // -----------------------------------------------------------------------
    // Protocol Constants
    // -----------------------------------------------------------------------

    enum : u16 {
        USBIP_VERSION = 0x0111   // v1.1.1 — current spec version
    };

    enum : u32 {
        USBIP_STATUS_OK = 0x00000000
    };

    // OP_REQ_* / OP_REP_* command codes (handshake phase)
    enum USBIP_Handshake_Command : u16 {
        OP_REQ_DEVLIST = 0x8005,
        OP_REP_DEVLIST = 0x0005,
        OP_REQ_IMPORT  = 0x8003,
        OP_REP_IMPORT  = 0x0003,
    };

    // USBIP_CMD_* / USBIP_RET_* command codes (URB phase)
    enum USBIP_URB_Command : u32 {
        USBIP_CMD_SUBMIT = 0x00000001,
        USBIP_RET_SUBMIT = 0x00000003,
        USBIP_CMD_UNLINK = 0x00000002,
        USBIP_RET_UNLINK = 0x00000004,
    };

    enum USBIP_Direction : u32 {
        USBIP_DIR_OUT = 0x00000000,
        USBIP_DIR_IN  = 0x00000001,
    };

    enum USBIP_Speed : u32 {
        USBIP_SPEED_LOW      = 1,  // USB 1.0 low-speed  (1.5 Mbps)
        USBIP_SPEED_FULL     = 2,  // USB 1.1 full-speed (12 Mbps)  ← ESP32-S2
        USBIP_SPEED_HIGH     = 3,  // USB 2.0 high-speed (480 Mbps)
        USBIP_SPEED_WIRELESS = 4,
        USBIP_SPEED_SUPER    = 5,  // USB 3.0 super-speed (5 Gbps)
    };

    // -----------------------------------------------------------------------
    // Handshake Phase — OP_REQ_DEVLIST / OP_REP_DEVLIST
    // -----------------------------------------------------------------------

    // Every OP_REQ_* and OP_REP_* message starts with this 8-byte header.
    // Read this first to identify the command, then read the rest.
    //
    //  Offset  Length  Description
    //  0x00    2       USBIP version (0x0111)
    //  0x02    2       Command code
    //  0x04    4       Status (0 = OK; unused in requests, set to 0)
    struct [[gnu::packed]] usbip_op_header {
        u16 version;
        u16 command;
        u32 status;
    };
    static_assert(sizeof(usbip_op_header) == 8);

    // OP_REP_DEVLIST header — extends usbip_op_header with a device count.
    // After this header, send `num_devices` × usbip_device_info, each followed
    // by bNumInterfaces × usbip_interface.
    //
    //  Offset  Length  Description
    //  0x00    8       usbip_op_header  (command = OP_REP_DEVLIST)
    //  0x08    4       Number of exported devices (0 if none)
    struct [[gnu::packed]] usbip_rep_devlist_header {
        u16 version;
        u16 command;      // = OP_REP_DEVLIST
        u32 status;
        u32 num_devices;
    };
    static_assert(sizeof(usbip_rep_devlist_header) == 12);

    // -----------------------------------------------------------------------
    // Handshake Phase — OP_REQ_IMPORT / OP_REP_IMPORT
    // -----------------------------------------------------------------------

    // OP_REQ_IMPORT — host requests to attach a specific device.
    //
    //  Offset  Length  Description
    //  0x00    2       USBIP version
    //  0x02    2       0x8003
    //  0x04    4       Status (unused, set to 0)
    //  0x08    32      busid — null-terminated, e.g. "1-1"
    struct [[gnu::packed]] usbip_req_import {
        u16  version;
        u16  command;    // = OP_REQ_IMPORT
        u32  status;
        char busid[32];
    };
    static_assert(sizeof(usbip_req_import) == 40);

    // Per-device info record — 312 bytes.
    //
    // Used in TWO places:
    //   1. Inside OP_REP_DEVLIST — one record per exported device (no OP header prefix).
    //   2. As the body of OP_REP_IMPORT (after the 8-byte op header).
    //
    // This is the plain device data WITHOUT the version/command/status prefix.
    // Do NOT send usbip_rep_import in the DEVLIST loop — send usbip_device_info.
    //
    //  Offset  Length  Description
    //  0x000   256     path — sysfs path, e.g. "/sys/bus/usb/devices/1-1"
    //  0x100   32      busid — e.g. "1-1"
    //  0x120   4       busnum
    //  0x124   4       devnum
    //  0x128   4       speed  (USBIP_Speed)
    //  0x12C   2       idVendor
    //  0x12E   2       idProduct
    //  0x130   2       bcdDevice
    //  0x132   1       bDeviceClass
    //  0x133   1       bDeviceSubClass
    //  0x134   1       bDeviceProtocol
    //  0x135   1       bConfigurationValue
    //  0x136   1       bNumConfigurations
    //  0x137   1       bNumInterfaces
    struct [[gnu::packed]] usbip_device_info {
        char        path[256];
        char        busid[32];
        u32         busnum;
        u32         devnum;
        USBIP_Speed speed;
        u16         idVendor;
        u16         idProduct;
        u16         bcdDevice;
        u8          bDeviceClass;
        u8          bDeviceSubClass;
        u8          bDeviceProtocol;
        u8          bConfigurationValue;
        u8          bNumConfigurations;
        u8          bNumInterfaces;
    };
    static_assert(sizeof(usbip_device_info) == 312);

    // OP_REP_IMPORT — our full response to OP_REQ_IMPORT.
    // On error, send only version + command + status (8 bytes), status != 0.
    // On success, send the full 320-byte struct, status = 0.
    //
    //  Offset  Length  Description
    //  0x00    8       usbip_op_header  (command = OP_REP_IMPORT)
    //  0x08    312     usbip_device_info  (only present if status == 0)
    struct [[gnu::packed]] usbip_rep_import {
        u16             version;
        u16             command;    // = OP_REP_IMPORT
        u32             status;
        usbip_device_info device;  // Only valid / only sent when status == 0
    };
    static_assert(sizeof(usbip_rep_import) == 320);

    // Per-interface descriptor — sent after each usbip_device_info,
    // repeated bNumInterfaces times.
    struct [[gnu::packed]] usbip_interface {
        u8 bInterfaceClass;
        u8 bInterfaceSubClass;
        u8 bInterfaceProtocol;
        u8 padding = 0;
    };
    static_assert(sizeof(usbip_interface) == 4);

    // -----------------------------------------------------------------------
    // URB Phase — common header
    // -----------------------------------------------------------------------

    // 20-byte header present at the start of every URB-phase message
    // (CMD_SUBMIT, CMD_UNLINK, RET_SUBMIT, RET_UNLINK).
    //
    //  Offset  Length  Description
    //  0x00    4       command
    //  0x04    4       seqnum — monotonically increasing per connection
    //  0x08    4       devid — (busnum << 16 | devnum) in requests; 0 in responses
    //  0x0C    4       direction — USBIP_DIR_OUT / USBIP_DIR_IN; 0 in responses
    //  0x10    4       ep — endpoint number 0-15; 0 in responses and UNLINK
    struct [[gnu::packed]] usbip_header_basic {
        USBIP_URB_Command command;
        u32               seqnum;
        u32               devid;
        USBIP_Direction   direction;
        u32               ep;
    };
    static_assert(sizeof(usbip_header_basic) == 20);

    // -----------------------------------------------------------------------
    // URB Phase — USBIP_CMD_SUBMIT (host → device)
    // -----------------------------------------------------------------------
    //
    // Followed immediately on the wire by:
    //   - transfer_buffer_length bytes of OUT data  (if direction == OUT)
    //   - iso_packet_descriptor array               (if ISO transfer)
    //   - nothing                                   (if direction == IN, non-ISO)
    //
    //  Offset  Length  Description
    //  0x00    20      usbip_header_basic (command = 0x00000001)
    //  0x14    4       transfer_flags
    //  0x18    4       transfer_buffer_length
    //  0x1C    4       start_frame (0 if not ISO)
    //  0x20    4       number_of_packets (0xFFFFFFFF if not ISO)
    //  0x24    4       interval
    //  0x28    8       setup data (8 USB setup bytes; zeroed if not a setup transfer)
    struct [[gnu::packed]] usbip_cmd_submit {
        usbip_header_basic header;
        u32 transfer_flags;
        u32 transfer_buffer_length;
        u32 start_frame;
        u32 number_of_packets;
        u32 interval;
        u8  setup[8];
    };
    static_assert(sizeof(usbip_cmd_submit) == 48);

    // -----------------------------------------------------------------------
    // URB Phase — USBIP_RET_SUBMIT (device → host)
    // -----------------------------------------------------------------------
    //
    // Followed immediately on the wire by:
    //   - actual_length bytes of IN data    (if direction == IN)
    //   - iso_packet_descriptor array       (if ISO transfer)
    //   - nothing                           (if direction == OUT, non-ISO)
    //
    //  Offset  Length  Description
    //  0x00    20      usbip_header_basic (command = 0x00000003)
    //  0x14    4       status (0 = success; errno on error, e.g. 32 = EPIPE = STALL)
    //  0x18    4       actual_length
    //  0x1C    4       start_frame (0 if not ISO)
    //  0x20    4       number_of_packets (0xFFFFFFFF if not ISO)
    //  0x24    4       error_count
    //  0x28    8       padding — SHALL BE ZERO  ← critical: missing this breaks sync
    struct [[gnu::packed]] usbip_ret_submit {
        usbip_header_basic header;
        u32 status;
        u32 actual_length;
        u32 start_frame;
        u32 number_of_packets;
        u32 error_count;
        u8  padding[8];          // spec-mandated, shall be 0
    };
    static_assert(sizeof(usbip_ret_submit) == 48);

    // -----------------------------------------------------------------------
    // URB Phase — USBIP_CMD_UNLINK (host → device)
    // -----------------------------------------------------------------------
    //
    // Requests cancellation of a previously submitted URB.
    // If the URB hasn't completed yet: cancel it, send RET_UNLINK with
    //   status = -ECONNRESET, and do NOT send RET_SUBMIT for the cancelled URB.
    // If the URB already completed: send RET_UNLINK with status = 0.
    //   (The host will not receive a second RET_SUBMIT for it.)
    //
    //  Offset  Length  Description
    //  0x00    20      usbip_header_basic (command = 0x00000002)
    //  0x14    4       unlink_seqnum — seqnum of the CMD_SUBMIT to cancel
    //  0x18    24      padding — shall be set to 0
    struct [[gnu::packed]] usbip_cmd_unlink {
        usbip_header_basic header;
        u32 unlink_seqnum;
        u8  padding[24];
    };
    static_assert(sizeof(usbip_cmd_unlink) == 48);

    // -----------------------------------------------------------------------
    // URB Phase — USBIP_RET_UNLINK (device → host)
    // -----------------------------------------------------------------------
    //
    //  Offset  Length  Description
    //  0x00    20      usbip_header_basic (command = 0x00000004)
    //  0x14    4       status: -ECONNRESET if successfully unlinked; 0 if already done
    //  0x18    24      padding — shall be set to 0
    struct [[gnu::packed]] usbip_ret_unlink {
        usbip_header_basic header;
        u32 status;
        u8  padding[24];
    };
    static_assert(sizeof(usbip_ret_unlink) == 48);

} // namespace lm::usbip
