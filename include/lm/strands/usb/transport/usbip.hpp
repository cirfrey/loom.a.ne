#pragma once

// Managed strand that runs the USB/IP TCP server.
// Spawn this instead of (or alongside) the hardware USB strand on native builds.

#include "lm/chip/types.hpp"
#include "lm/usbd/usbip_types.hpp"
#include "lm/usb/common.hpp"

#include "lm/config.hpp"
#include "lm/fabric/all.hpp"

#include <array>

namespace lm::strands::usbip_backend
{
    [[maybe_unused]] static std::array<
        lm::usb::ep_t,
        config_t::usbip_t::max_endpoints
    > endpoints = {{
        // Only init the first as control, the rest are defalt-initialized as unassigned.
        lm::usb::ep_t{
            .in      = lm::usb::ept_t::control,
            .in_itf  = lm::usb::itf_t::control,
            .out     = lm::usb::ept_t::control,
            .out_itf = lm::usb::itf_t::control
        }
    }};
}

namespace lm::strands::usb::transport
{
    struct usbip
    {
        using ri     = fabric::strand::strand_runtime_info;
        using status = fabric::strand::managed_strand_status;
        using cfg_t  = config_t::usbip_t;

        struct find_cfg_args
        {
            u8 id = 0;
            u64 tries = 3;
            u64 timeout = 0;
            u32 name_hash = 0; // If you already have the name_hash and want to just get
                               // the pointer.
        };
        static auto find_cfg(find_cfg_args) -> cfg_t const*;

        ri& info;
        cfg_t const* cfg;

        usbip(ri& info, cfg_t const* cfg = nullptr);
        auto on_ready()     -> status;
        auto before_sleep() -> status;
        auto on_wake()      -> status;
        ~usbip();

        enum state_t : u8 {
            initializing, // Before the listen socket is open and device is attached.
            listening,    // Socket is open and device is attached, waiting for a TCP connection.
            handshaking,  // Connected: handling OP_REQ_DEVLIST / OP_REQ_IMPORT.
            exported,     // OP_REP_IMPORT sent — about to bind DCD and let TinyUSB take over.
            transmitting, // DCD bound, URB loop running.

            state_count
        };

        auto transition_state(state_t to) -> bool;

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

        /// --- Handshaking ---
        auto handshaking_read_header() -> void;
        auto handshaking_read_busid() -> void;
        auto handshaking_process_req_devlist() -> void;
        auto handshaking_process_req_import() -> void;

        /// --- Transmitting ---
        // TODO: investigate the whole transmitting state, this logic seems a little contrived.
        auto transmitting_read_urb() -> bool;
        auto transmitting_handle_setup(
            const lm::usbip::usbip_cmd_submit&,
            u32 seqnum,
            u8 ep_addr
        ) -> bool;
        auto transmitting_handle_out(
            const lm::usbip::usbip_cmd_submit&,
            u32 seqnum,
            u8 ep_addr
        ) -> bool;
        auto transmitting_handle_in(
            const lm::usbip::usbip_cmd_submit&,
            u32 seqnum,
            u8 ep_addr
        ) -> bool;
        auto transmitting_handle_unlink(const lm::usbip::usbip_cmd_unlink&) -> bool;
        // Flush all events from in_event_q, matching them to parked IN requests
        // or buffering them for the next poll.
        // Returns false on connection loss.
        auto transmitting_flush_inbound_events() -> bool;
        // Serialise a bus event into a USB payload.
        // Returns 0 if this event should not produce a USB transfer on usbip.
        auto serialise_event_to_usb(const fabric::event& e,
                                    u8* out_buf, u8 ep_addr_out[1]) -> u16;
        // SETUP request handlers (called from transmitting_handle_setup).
        auto setup_handle_get_descriptor(const u8 setup[8], u32 seqnum) -> bool;
        auto setup_handle_set_address   (const u8 setup[8], u32 seqnum) -> bool;
        auto setup_handle_set_config    (const u8 setup[8], u32 seqnum) -> bool;
        auto setup_handle_class_request (const u8 setup[8], u32 seqnum,
                                         u32 ep_addr)                   -> bool;

        auto send_ret_submit(u32 seqnum, u32 ep_num, u32 direction,
                             u32 status, const void* data, u32 len)     -> bool;


        // For organization purposes, if a state has complex logic, requires persistent
        // data, or has some subfunctions it should communicate with the do_*_state
        // via this union and NOT change the state itself.
        // Managing transitions is exclusive to do_*_state.
        union state_data_t
        {
            // The construction and destruction is managed by transition_state().
            state_data_t() : initializing{} {}
            ~state_data_t() {};

            // For states that dont require any additional data.
            struct none_t {};
            struct listening_t    : none_t {};
            struct exported_t     : none_t {};

            struct initializing_t
            {
                bool device_attached = false;
                u8 device_attach_seqnum = 0;
                u64 attach_request_sent_timestamp = 0;
                u64 config_descriptor_request_sent = 0;
                u64 device_descriptor_request_sent = 0;
            } initializing;

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
                pending_in_t pending_in[cfg_t::max_endpoints] = {};

                struct buffered_data_t {
                    u8   data[64] = {};
                    u16  len      = 0;
                    bool ready    = false;
                };
                buffered_data_t buffered_out[cfg_t::max_endpoints] = {};

                fabric::queue_t              out_event_q   = {};
                fabric::bus::subscribe_token out_event_tok = {};

                u8 address = 0;

                transmitting_t(cfg_t const* cfg) {
                    out_event_q   = fabric::queue<fabric::event>(cfg->out_event_queue_size);
                    out_event_tok = fabric::bus::subscribe(out_event_q, fabric::topic::output);
                }
            } transmitting;
        };

        // And state-independent data.
        state_t        state       = initializing;
        state_data_t   state_data  = {};

        chip::socket_t listen_sock = chip::invalid_socket;
        chip::socket_t conn_sock   = chip::invalid_socket;

        // Cached from the device.
        u8 config_descriptor[cfg_t::config_descriptor_max_size] = {0};
        u16 config_descriptor_size = 0;
        struct string_descriptor
        {
            static constexpr u16 unassigned = ~u16(0);

            cfg_t::string_descriptor value = {0};
            u16 idx = unassigned;

            constexpr bool is_unassigned() const { return idx == unassigned; }
        } string_descriptors[cfg_t::string_descriptors_max_size] = {0};
        u16 string_descriptors_size = 0;
        lm::usb::dd device_descriptor = lm::usb::dd::zero();

        // For talking to the device.
        u8 device_strand_id = 0;
        u8 device_loom_id = 0;
        u8 device_mesh_id = 0;
        fabric::queue_t device_q = {};
        fabric::bus::subscribe_token device_tok = {};
    };
}
