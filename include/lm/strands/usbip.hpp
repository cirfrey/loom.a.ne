#pragma once

// Managed strand that runs the USB/IP TCP server.
// Spawn this instead of (or alongside) the hardware USB strand on native builds.

#include "lm/fabric/types.hpp"
#include "lm/chip/types.hpp"
#include "lm/usbd/usbip_types.hpp"

#include "lm/config.hpp"

#include <common/tusb_types.h>

namespace lm::strands
{
    struct usbip
    {
        usbip(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~usbip();

        enum state_t {
            initializing, // Before the listen socket is open.
            listening,    // Socket open, waiting for a TCP connection.
            handshaking,  // Connected: handling OP_REQ_DEVLIST / OP_REQ_IMPORT.
            exported,     // OP_REP_IMPORT sent — about to bind DCD and let TinyUSB take over.
            transmitting, // DCD bound, URB loop running.
        };

        // What does the state want the strand to do.
        enum class desired_strand_action
        {
            loop,
            yield,
            die,
        };
        auto do_initializing_state() -> desired_strand_action;
        auto do_listening_state()    -> desired_strand_action;
        auto do_handshaking_state()  -> desired_strand_action;
        auto do_exported_state()     -> desired_strand_action;
        auto do_transmitting_state() -> desired_strand_action;

        // Subfunctions specific to a given state.
        auto handshaking_read_header() -> void;
        auto handshaking_read_busid() -> void;
        auto handshaking_process_req_devlist() -> void;
        auto handshaking_process_req_import() -> void;

        // For organization purposes, if a state has complex logic, requires persistent
        // data, or has some subfunctions it should communicate with the do_*_state
        // via this union and NOT change the state itself.
        // Managing transitions is exclusive to do_*_state.
        union state_data_t
        {
            // For states that dont require any additional data.
            struct none_t {};
            using initializing_t = none_t;
            using listening_t    = none_t;
            using exported_t     = none_t;

            initializing_t initializing;
            listening_t    listening;
            exported_t     exported;

            struct handshaking_t
            {
                lm::usbip::usbip_req_import packet;
                enum handshaking_status : u8 {
                    // Common for both OP_REQ_DEVLIST and OP_REQ_IMPORT.
                    waiting_for_header,
                    // If the packet is OP_REQ_IMPORT.
                    waiting_for_busid,

                    want_to_process_req_devlist,
                    want_to_process_req_import,
                    want_to_go_back_to_listening,
                    want_to_go_forward_to_exported,

                    unkown_command_error,
                    header_recv_error,
                    busid_recv_error,
                    devlist_header_send_error,
                    devlist_device_send_error,
                    devlist_interface_send_error,
                    import_send_error,
                } status = waiting_for_header;
            } handshaking;

            struct transmitting_t
            {
                struct pending_in_t {
                    u32  seqnum  = 0;
                    u16  max_len = 0;    // transfer_buffer_length from CMD_SUBMIT
                    bool parked  = false;
                };

                struct buffered_data_t {
                    u8   data[64] = {};
                    u16  len      = 0;
                    bool ready    = false;
                };

                pending_in_t    pending_in  [config_t::usbip_t::max_endpoints] = {};
                buffered_data_t buffered_out[config_t::usbip_t::max_endpoints] = {};
            } transmitting;
        };

        // And state-independent data.
        state_t        state       = initializing;
        state_data_t   state_data  = {};

        chip::socket_t listen_sock = chip::invalid_socket;
        chip::socket_t conn_sock   = chip::invalid_socket;

        tusb_desc_device_t device_desc = {}; // This is all we use tinyusb for here, it's convenient.
        u8 config_desc[config_t::usbip_t::config_descriptor_max_size] = {};
        u8 dev_address = 0;   // assigned by host during SET_ADDRES
    };
}
