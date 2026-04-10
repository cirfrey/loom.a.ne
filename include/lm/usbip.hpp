#include "lm/core/types.hpp"

namespace lm::usbip
{
    // --- Protocol Constants ---
    enum : u16 {
        USBIP_VERSION = 0x0111
    };

    enum : u32 {
        USBIP_STATUS_OK = 0x00000000
    };

    // --- Handshake Commands ---
    enum USBIP_Handshake_Command : u16 {
        OP_REQ_DEVLIST = 0x8005,
        OP_REP_DEVLIST = 0x0005,
        OP_REQ_IMPORT  = 0x8003,
        OP_REP_IMPORT  = 0x0003
    };

    // --- URB Commands ---
    enum USBIP_URB_Command : u32 {
        USBIP_CMD_SUBMIT = 0x00000001,
        USBIP_RET_SUBMIT = 0x00000003,
        USBIP_CMD_UNLINK = 0x00000002,
        USBIP_RET_UNLINK = 0x00000004
    };

    // --- Direction ---
    enum USBIP_Direction : u32 {
        USBIP_DIR_OUT = 0x00000000,
        USBIP_DIR_IN  = 0x00000001
    };

    // --- Speed Mapping ---
    enum USBIP_Speed : u32 {
        USBIP_SPEED_LOW      = 1,
        USBIP_SPEED_FULL     = 2,
        USBIP_SPEED_HIGH     = 3,
        USBIP_SPEED_WIRELESS = 4,
        USBIP_SPEED_SUPER    = 5
    };

    // --- Handshake Phase Structs ---

    struct [[gnu::packed]] usbip_req_import {
        u16 version;
        USBIP_Handshake_Command command;
        u32 status;
        char busid[32];
    };
    static_assert(sizeof(usbip_req_import) == 40, "usbip_req_import size mismatch");

    struct [[gnu::packed]] usbip_rep_import {
        u16 version;
        USBIP_Handshake_Command command;
        u32 status;

        char path[256];
        char busid[32];
        u32 busnum;
        u32 devnum;
        USBIP_Speed speed;

        u16 idVendor;
        u16 idProduct;
        u16 bcdDevice;
        u8  bDeviceClass;
        u8  bDeviceSubClass;
        u8  bDeviceProtocol;
        u8  bConfigurationValue;
        u8  bNumConfigurations;
        u8  bNumInterfaces;
    };
    static_assert(sizeof(usbip_rep_import) == 320, "usbip_rep_import size mismatch");

    struct [[gnu::packed]] usbip_interface {
        u8 bInterfaceClass;
        u8 bInterfaceSubClass;
        u8 bInterfaceProtocol;
        u8 padding;
    };
    static_assert(sizeof(usbip_interface) == 4, "usbip_interface size mismatch");

    // --- URB Transfer Phase Structs ---

    struct [[gnu::packed]] usbip_header_basic {
        USBIP_URB_Command command;
        u32 seqnum;
        u32 devid;
        USBIP_Direction direction;
        u32 ep;
    };
    static_assert(sizeof(usbip_header_basic) == 20, "usbip_header_basic size mismatch");

    struct [[gnu::packed]] usbip_cmd_submit {
        usbip_header_basic header;
        u32 transfer_flags;
        u32 transfer_buffer_length;
        u32 start_frame;
        u32 number_of_packets;
        u32 interval;
        u8  setup[8];
    };
    static_assert(sizeof(usbip_cmd_submit) == 48, "usbip_cmd_submit size mismatch");

    struct [[gnu::packed]] usbip_ret_submit {
        usbip_header_basic header;
        u32 status;
        u32 actual_length;
        u32 start_frame;
        u32 number_of_packets;
        u32 error_count;
    };
    static_assert(sizeof(usbip_ret_submit) == 40, "usbip_ret_submit size mismatch");

}
