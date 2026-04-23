#include "lm/strands/usbip.hpp"

#include "lm/chip/net.hpp"
#include "lm/core/endian.hpp"
#include "lm/core/cvt.hpp"
#include "lm/board.hpp"
#include "lm/log.hpp"

#include <cstdio>

#include "lm/usb/backend.hpp"
#include "lm/usb/debug.hpp"

lm::strands::usbip::usbip(ri& info) : info{info}
{
    auto lens = usb::backend::setup_descriptors(
        config_descriptor,
        string_descriptors,
        device_descriptor,
        config.usbip,
        config.audio.backend.usbip,
        config.cdc.backend.usbip,
        config.hid.backend.usbip,
        config.midi.backend.usbip,
        config.msc.backend.usbip
    );
    config_descriptor_size = lens.total;

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
        log::fmt_t(log::fmt_t_args::from_config()
            .with_fmt(fmt)
            .with_timestamp(log::timestamp_t::no_timestamp)
            .with_filename(log::filename_t::no_filename)
            .with_prefix(log::prefix_t::disabled)
        ),
        veil::forward<decltype(args)>(args)...
    ); };
    usb::debug::print_ep_table(config.usbip.endpoints, printer, {"\t", 1});
}

auto lm::strands::usbip::on_ready() -> status
{ return status::ok; }

auto lm::strands::usbip::before_sleep() -> status
{ return status::ok; }

// Just a very simple state machine.
auto lm::strands::usbip::on_wake() -> status
{
    using state_func_t = desired_strand_action(usbip::*)();
    state_func_t state_funcs[] = {
        [initializing] = &usbip::do_initializing_state,
        [listening]    = &usbip::do_listening_state,
        [handshaking]  = &usbip::do_handshaking_state,
        [exported]     = &usbip::do_exported_state,
        [transmitting] = &usbip::do_transmitting_state,
    };
    while(1)
    {
        switch((this->*state_funcs[state])())
        {
            case desired_strand_action::loop:  continue;
            case desired_strand_action::yield: return status::ok;
            case desired_strand_action::die:   return status::suicidal;
            default:                           return status::ok;
        }
    }

    return status::ok;
}

lm::strands::usbip::~usbip() { transition_state(initializing); }

/// State stuff.

auto lm::strands::usbip::transition_state(state_t to) -> bool
{
    if (state == to) return true;

    if(state > listening && to <= listening)
    {
        chip::net::close(conn_sock);
        conn_sock = chip::invalid_socket;
    }

    if(state > initializing && to == initializing)
    {
        chip::net::close(listen_sock);
        listen_sock = chip::invalid_socket;
    }

    switch(state)
    {
        case initializing: { state_data.initializing.~initializing_t(); break; }
        case listening:    { state_data.listening.~listening_t();       break; }
        case handshaking:  { state_data.handshaking.~handshaking_t();   break; }
        case exported:     { state_data.exported.~exported_t();         break; }
        case transmitting: { state_data.transmitting.~transmitting_t(); break; }
        default:           { return false; }
    }

    state = to;
    switch(state)
    {
        case initializing: { new (&state_data.initializing) state_data_t::initializing_t(); break; }
        case listening:    { new (&state_data.listening)    state_data_t::listening_t();    break; }
        case handshaking:  { new (&state_data.handshaking)  state_data_t::handshaking_t();  break; }
        case exported:     { new (&state_data.exported)     state_data_t::exported_t();     break; }
        case transmitting: { new (&state_data.transmitting) state_data_t::transmitting_t(); break; }
        default:           { return false; }
    }

    return true;
}

auto lm::strands::usbip::do_initializing_state() -> desired_strand_action
{
    listen_sock = chip::net::make_listen_socket(config.usbip.port);
    if (listen_sock == chip::invalid_socket) {
        log::error("[usbip] Failed to open listen socket on port %i\n", config.usbip.port);
        return desired_strand_action::die;
    }

    chip::net::set_nonblocking(listen_sock);
    log::info("[usbip] Listening on port %i\n", config.usbip.port);
    transition_state(listening);
    return desired_strand_action::loop;
}

auto lm::strands::usbip::do_listening_state() -> desired_strand_action
{
    conn_sock = chip::net::try_accept(listen_sock);
    if (conn_sock == chip::invalid_socket) return desired_strand_action::yield;

    chip::net::set_nodelay(conn_sock);
    log::info("[usbip] Client connected — starting handshake\n");
    transition_state(handshaking);
    return desired_strand_action::loop;
}

auto lm::strands::usbip::do_handshaking_state() -> desired_strand_action
{
    auto& data = state_data.handshaking;

    auto const should_read_header =
        data.status == data.waiting_for_header &&
        chip::net::avail(conn_sock) >= sizeof(lm::usbip::usbip_op_header);
    if(should_read_header) handshaking_read_header();

    auto const should_read_busid =
        data.status == data.waiting_for_busid &&
        chip::net::avail(conn_sock) >= sizeof(data.packet.busid);
    if(should_read_busid) handshaking_read_busid();

    if(data.status == data.want_to_process_req_devlist) handshaking_process_req_devlist();
    if(data.status == data.want_to_process_req_import)  handshaking_process_req_import();

    // Handlers for misc statuses.
    auto const status_is_error =
        data.status == data.unkown_command_error ||
        data.status == data.header_recv_error ||
        data.status == data.busid_recv_error ||
        data.status == data.devlist_header_send_error ||
        data.status == data.devlist_device_send_error ||
        data.status == data.devlist_interface_send_error ||
        data.status == data.import_send_error;
    auto const should_go_back_to_listening =
        data.status == data.want_to_go_back_to_listening ||
        status_is_error;
    if(should_go_back_to_listening)
    {
        log::info("[usbip] Going back to listening\n");
        transition_state(listening);
        return desired_strand_action::loop;
    }

    if(data.status == data.want_to_go_forward_to_exported)
    {
        transition_state(exported);
        return desired_strand_action::loop;
    }

    // At this point, the only substates this can be is waiting_for_header
    // or waiting_for_busid, so we might as well yield.
    return desired_strand_action::yield;
}

auto lm::strands::usbip::handshaking_read_header() -> void
{
    auto& data = state_data.handshaking;

    if(!chip::net::recv_exact(
        conn_sock,
        &data.packet,
        sizeof(lm::usbip::usbip_op_header)
    ))
    {
        log::error("[usbip] Error while receiving the HEADER during handshaking\n");
        data.status = data.header_recv_error;
        return;
    }

    auto command = ntoh16(data.packet.command);
    if(command == lm::usbip::OP_REQ_DEVLIST)
    { data.status = data.want_to_process_req_devlist; }
    else if(command == lm::usbip::OP_REQ_IMPORT)
    { data.status = data.waiting_for_busid; }
    else
    {
        log::error("[usbip] Unknown URB command 0x%08x\n", command);
        data.status = data.unkown_command_error;
        return;
    }
}

auto lm::strands::usbip::handshaking_read_busid() -> void
{
    auto& data = state_data.handshaking;

    if(!chip::net::recv_exact(
        conn_sock,
        &data.packet.busid,
        sizeof(data.packet.busid)
    ))
    {
        log::error("[usbip] Error while receiving BUSID during handshaking\n");
        data.status = data.busid_recv_error;
        return;
    }

    data.status = data.want_to_process_req_import;
}

// This is split into 3 parts. The common header, then for each device (we only
// support 1) we send a device information struct and for each interface in the
// device we send a interface information struct.
auto lm::strands::usbip::handshaking_process_req_devlist() -> void
{
    auto& data = state_data.handshaking;

    /// Header.
    auto header = lm::usbip::usbip_rep_devlist_header{
        .version     = hton16(lm::usbip::USBIP_VERSION),
        .command     = hton16(lm::usbip::OP_REP_DEVLIST),
        .status      = 0,
        .num_devices = hton32(1),
    };
    if(!chip::net::send_exact(conn_sock, &header, sizeof(header)))
    {
        log::error("[usbip] Error while sending the DEVLIST_REP_HEADER during handshaking\n");
        data.status = data.devlist_header_send_error;
        return;
    }

    /// Device.
    auto dev_info = lm::usbip::usbip_device_info{
        .path                = {'\0'}, // snprintf later.
        .busid               = {'\0'}, // snprintf later.
        .busnum              = hton32(1),
        .devnum              = hton32(1),
        .speed               = (lm::usbip::USBIP_Speed)hton32(lm::usbip::USBIP_SPEED_FULL),
        .idVendor            = hton16(device_descriptor.idVendor),
        .idProduct           = hton16(device_descriptor.idProduct),
        .bcdDevice           = hton16(device_descriptor.bcdDevice),
        .bDeviceClass        = device_descriptor.bDeviceClass,
        .bDeviceSubClass     = device_descriptor.bDeviceSubClass,
        .bDeviceProtocol     = device_descriptor.bDeviceProtocol,
        .bConfigurationValue = 1,
        .bNumConfigurations  = device_descriptor.bNumConfigurations,
        .bNumInterfaces      = config_descriptor[4],
    };
    std::snprintf(dev_info.path,  sizeof(dev_info.path),  config.usbip.path);
    std::snprintf(dev_info.busid, sizeof(dev_info.busid), config.usbip.busid);

    if (!chip::net::send_exact(conn_sock, &dev_info, sizeof(dev_info)))
    {
        log::error("[usbip] Error while sending the DEVLIST_REP_DEVICEINFO during handshaking\n");
        data.status = data.devlist_device_send_error;
        return;
    }

    /// Interfaces.
    const u8* p   = config_descriptor;
    const u8* end = p + ((u16)config_descriptor[2] | ((u16)config_descriptor[3] << 8));
    p += config_descriptor[0]; // skip config descriptor itself
    u8 itf_count = 0;
    while (p < end && itf_count < dev_info.bNumInterfaces) {
        if (p[1] != TUSB_DESC_INTERFACE)
        {
            p += p[0];
            continue;
        }

        lm::usbip::usbip_interface itf_info{};
        itf_info.bInterfaceClass    = p[5];
        itf_info.bInterfaceSubClass = p[6];
        itf_info.bInterfaceProtocol = p[7];
        itf_info.padding            = 0;
        if (!chip::net::send_exact(conn_sock, &itf_info, sizeof(itf_info)))
        {
            log::error("[usbip] Error while sending the DEVLIST_REP_INTERFACE during handshaking\n");
            data.status = data.devlist_interface_send_error;
            return;
        }
        ++itf_count;
        p += p[0]; // advance by bLength
    }

    // Now we hopefully wait for OP_REQ_IMPORT, but we *may* need to close the socket first.
    log::info("[usbip] Sucessfully sent OP_REP_DEVLIST\n");
    data.status = config.usbip.close_conn_after_devlist == feature::on
        ? data.want_to_go_back_to_listening
        : data.waiting_for_header;
}

// Pretty similar to OP_REQ_DEVLIST, we just skip the interfaces and have a slighly
// different header.
auto lm::strands::usbip::handshaking_process_req_import() -> void
{
    auto& data = state_data.handshaking;

    auto pkt = lm::usbip::usbip_rep_import{
        .version  = hton16(lm::usbip::USBIP_VERSION | tou),
        .command  = hton16(lm::usbip::OP_REP_IMPORT | tou),
        .status   = 0, // OK
        .device = {
            .path                = {'\0'}, // snprintf later.
            .busid               = {'\0'}, // snprintf later.
            .busnum              = hton32(1),
            .devnum              = hton32(1),
            .speed               = (lm::usbip::USBIP_Speed)hton32(lm::usbip::USBIP_SPEED_FULL),
            .idVendor            = hton16(device_descriptor.idVendor),
            .idProduct           = hton16(device_descriptor.idProduct),
            .bcdDevice           = hton16(device_descriptor.bcdDevice),
            .bDeviceClass        = device_descriptor.bDeviceClass,
            .bDeviceSubClass     = device_descriptor.bDeviceSubClass,
            .bDeviceProtocol     = device_descriptor.bDeviceProtocol,
            .bConfigurationValue = 1,
            .bNumConfigurations  = device_descriptor.bNumConfigurations,
            .bNumInterfaces      = config_descriptor[4],
        }
    };
    std::snprintf(pkt.device.path,  sizeof(pkt.device.path),  config.usbip.path);
    std::snprintf(pkt.device.busid, sizeof(pkt.device.busid), config.usbip.busid);

    if (!chip::net::send_exact(conn_sock, &pkt, sizeof(pkt)))
    {
        log::error("[usbip] Error while sending the DEVLIST_REP_IMPORT during handshaking\n");
        data.status = data.import_send_error;
        return;
    }

    log::info("[usbip] Sucessfully sent OP_REP_IMPORT\n");
    data.status = data.want_to_go_forward_to_exported;
}

auto lm::strands::usbip::do_exported_state() -> desired_strand_action
{
    // OP_REP_IMPORT has been sent — the Linux kernel now thinks we are a
    // USB device. Bind the DCD so TinyUSB starts processing events.
    // bind_port() fires dcd_event_bus_reset() which kicks TinyUSB into
    // its enumeration sequence (GET_DESCRIPTOR etc.), driven by the URBs
    // that start arriving on conn_sock.
    transition_state(transmitting);
    log::info("[usbip] Attached! Starting URB loop and listening to events\n");
    return desired_strand_action::loop;
}

auto lm::strands::usbip::do_transmitting_state() -> desired_strand_action
{
    auto disconnect_and_listen = [&](bool detached = false){
        if(!detached) log::warn("[usbip] Connection lost - returning to listening\n");
        else          log::debug("[usbip] Detached - returning to listening\n");
        transition_state(listening);
        return desired_strand_action::loop;
    };

    // Step 1: drain the bus subscription queue.
    //
    // Each event in in_event_q is something a sibling strand wants to send
    // to the host (a MIDI message, HID report, etc.).
    //
    // For each event:
    //   - Serialise it to a USB payload (e.g. USB MIDI 4-byte packet, HID report bytes)
    //   - Determine which IN endpoint it belongs to
    //   - If the host has already polled that endpoint (pending_in[ep] is parked):
    //       → reply immediately with RET_SUBMIT, clear the slot
    //   - If not (host hasn't polled yet):
    //       → buffer the payload in buffered_out[ep] and wait for the next CMD_SUBMIT
    if (!transmitting_flush_inbound_events()) return disconnect_and_listen();

    auto& data = state_data.transmitting;
    // Step 2: for any endpoint that has buffered data AND a pending IN request,
    // send it now. (Handles the race where step 1 found no pending_in but one
    // arrived in the same wake after we already processed the queue.)
    for (u8 i = 0; i < config_t::usbip_t::max_endpoints; i++) {
        if (!data.pending_in[i].parked) continue;
        if (!data.buffered_out[i].ready) continue;

        if (!send_ret_submit(
            data.pending_in[i].seqnum, i, lm::usbip::USBIP_DIR_IN,
            0, data.buffered_out[i].data, data.buffered_out[i].len
        )) return disconnect_and_listen();

        data.pending_in[i]   = {};
        data.buffered_out[i] = {};
    }

    // Step 3: read an URB from the socket if enough bytes are available.
    if(chip::net::avail(conn_sock) >= sizeof(lm::usbip::usbip_header_basic))
    {
        // TODO: handle OP_REQ_UNIMPORT.
        if (transmitting_read_urb()) return desired_strand_action::loop;
        else return disconnect_and_listen();
    }

    return desired_strand_action::yield;
}


// Read exactly one URB (CMD_SUBMIT or CMD_UNLINK) from conn_sock and dispatch.
auto lm::strands::usbip::transmitting_read_urb() -> bool
{
    // All URB messages start with the same 20-byte basic header.
    lm::usbip::usbip_cmd_submit cmd{};
    if (!chip::net::recv_exact(conn_sock, &cmd.header, sizeof(cmd.header))) return false;

    const u32 command = ntoh32((u32)cmd.header.command);
    const u32 seqnum  = ntoh32(cmd.header.seqnum);
    const u32 ep_num  = ntoh32(cmd.header.ep);
    const u32 dir     = ntoh32((u32)cmd.header.direction);

    if (command == lm::usbip::USBIP_CMD_SUBMIT)
    {
        // Read the remaining 28 bytes of CMD_SUBMIT (after the 20-byte header).
        const st extra = sizeof(lm::usbip::usbip_cmd_submit) - sizeof(lm::usbip::usbip_header_basic);
        if (!chip::net::recv_exact(conn_sock, (u8*)&cmd + sizeof(cmd.header), extra)) return false;

        auto const is_in = (dir == lm::usbip::USBIP_DIR_IN);
        // Check if the 8 setup bytes are non-zero.
        // If they are, and we are on EP0, this is a Control Transfer (SETUP).
        auto const is_setup = ep_num == 0 && (*(u64*)cmd.setup != 0);

        if      (is_setup) return transmitting_handle_setup(cmd, seqnum, ep_num);
        else if (is_in)    return transmitting_handle_in   (cmd, seqnum, ep_num);
        else               return transmitting_handle_out  (cmd, seqnum, ep_num);
    }
    else if (command == lm::usbip::USBIP_CMD_UNLINK)
    {
        // CMD_UNLINK has the same total size as CMD_SUBMIT. Read the rest.
        lm::usbip::usbip_cmd_unlink unlink{};
        unlink.header = cmd.header;
        const st extra = sizeof(lm::usbip::usbip_cmd_unlink) - sizeof(lm::usbip::usbip_header_basic);
        if (!chip::net::recv_exact(conn_sock, (u8*)&unlink + sizeof(unlink.header), extra)) return false;

        return transmitting_handle_unlink(unlink);
    }
    else
    {
        log::warn("[usbip] Unknown URB command 0x%08x — disconnecting\n", (unsigned)command);
        return false;
    }
}

auto lm::strands::usbip::transmitting_handle_unlink(const lm::usbip::usbip_cmd_unlink& unlink) -> bool
{
    auto& data = state_data.transmitting;
    const u32 seqnum_to_unlink = ntoh32(unlink.unlink_seqnum);
    bool found = false;

    // 1. Check our parked IN requests
    for (u8 i = 0; i < config_t::usbip_t::max_endpoints; i++) {
        if (data.pending_in[i].parked && data.pending_in[i].seqnum == seqnum_to_unlink) {
            log::debug("[usbip] Unlinking parked IN URB (seqnum: %u) on EP %u\n", seqnum_to_unlink, i);

            // Clear the parked request
            data.pending_in[i] = {};
            found = true;
            break;
        }
    }

    // 2. Respond to the UNLINK request
    // According to USBIP spec, we return a RET_UNLINK packet.
    // The status should be:
    //  0          - If we successfully unlinked the URB.
    // -ECONNRESET - (or similar non-zero) if the URB was already finished or not found.
    lm::usbip::usbip_ret_unlink ret{};
    ret.header.command   = (lm::usbip::USBIP_URB_Command)hton32(lm::usbip::USBIP_RET_UNLINK);
    ret.header.seqnum    = unlink.header.seqnum; // Use the seqnum of the UNLINK command itself
    ret.header.devid     = 0;
    ret.header.direction = (lm::usbip::USBIP_Direction)hton32(lm::usbip::USBIP_DIR_OUT);
    ret.header.ep        = 0;

    // If found, status is 0 (success). If not found, it likely already completed.
    // In USBIP, -ECONNRESET is typically 104, but 0 is often used if it's "effectively" cancelled.
    ret.status = hton32(found ? 0 : 104);
    std::memset(ret.padding, 0, sizeof(ret.padding));

    return chip::net::send_exact(conn_sock, &ret, sizeof(ret));
}

auto lm::strands::usbip::transmitting_flush_inbound_events() -> bool
{
    auto& data = state_data.transmitting;

    // Drain all events currently in the queue — non-blocking.
    for (auto const& e : data.out_event_q.consume<fabric::event>())
    {
        u8  usb_payload[64] = {};
        u8  ep_addr = 0;

        // serialise_event_to_usb converts the bus event into raw USB bytes
        // and tells us which IN endpoint it belongs to.
        // Returns 0 if this event type has no USB representation on usbip.
        u16 payload_len = serialise_event_to_usb(e, usb_payload, &ep_addr);
        if (payload_len == 0) continue;

        u8 ep_idx = ep_addr & 0x7F;

        if (data.pending_in[ep_idx].parked) {
            // Host is waiting. Reply immediately.
            if (!send_ret_submit(
                data.pending_in[ep_idx].seqnum, ep_idx,
                lm::usbip::USBIP_DIR_IN,
                0, usb_payload, payload_len
            )) return false;
            data.pending_in[ep_idx] = {};
        } else {
            // Host hasn't polled yet. Buffer it.
            // Note: if buffered_out[ep_idx] is already ready we're producing
            // faster than the host polls — drop or ring-buffer here.
            if (!data.buffered_out[ep_idx].ready) {
                std::memcpy(data.buffered_out[ep_idx].data, usb_payload, payload_len);
                data.buffered_out[ep_idx].len   = payload_len;
                data.buffered_out[ep_idx].ready = true;
            }
        }
    }
    return true;
}

// Handle a SETUP CMD_SUBMIT (ep=0, dir=OUT, setup[8] non-zero).
//
// This is the SETUP stage of a USB control transfer (GET_DESCRIPTOR,
// SET_CONFIGURATION, SET_ADDRESS, etc.). The 8 setup bytes are in cmd.setup[].
//
// The reply (RET_SUBMIT with the descriptor data) is sent by
// transmitting_flush_pending_in() on the next tick, once the slot is occupied.
auto lm::strands::usbip::transmitting_handle_setup(
    const lm::usbip::usbip_cmd_submit& cmd, u32 seqnum, u8 ep_num) -> bool
{
    auto& data = state_data.transmitting;

    const u32 xfer_len  = ntoh32(cmd.transfer_buffer_length);
    const u32 direction = ntoh32((u32)cmd.header.direction);

    // 1. Drain any DATA OUT body (Data stage of a Control Write)
    // Most GET_DESCRIPTOR calls have xfer_len=0 in the CMD_SUBMIT header
    // because the data flows IN. But for SET_REPORT or similar, data follows.
    u8 data_stage_buffer[512];
    if (direction == lm::usbip::USBIP_DIR_OUT && xfer_len > 0) {
        u32 to_read = xfer_len > sizeof(data_stage_buffer) ? sizeof(data_stage_buffer) : xfer_len;
        if (!chip::net::recv_exact(conn_sock, data_stage_buffer, to_read)) return false;

        // Drain remaining if host sent more than our stack can handle
        // TODO: Ignoring the data is widely regarded as a bad move (tm).
        if (xfer_len > to_read) {
            u8 drain[64];
            u32 remaining = xfer_len - to_read;
            while (remaining > 0) {
                u32 chunk = remaining < sizeof(drain) ? remaining : sizeof(drain);
                if (!chip::net::recv_exact(conn_sock, drain, chunk)) return false;
                remaining -= chunk;
            }
        }
    }

    // 2. Parse the Setup Packet
    const u8 bmRequestType = cmd.setup[0];
    const u8 bRequest      = cmd.setup[1];

    // Determine request type (Standard = 0, Class = 1, Vendor = 2)
    const u8 type = (bmRequestType >> 5) & 0x03;

    using req = usb::standard_setup_request::standard_setup_request_t;
    if (type == 0) { // Standard Request
        switch (bRequest) {
            case req::get_descriptor:
                return setup_handle_get_descriptor(cmd.setup, seqnum);

            case req::set_address:
                // USBIP/Linux handles addressing, but we must ACK.
                data.address = cmd.setup[2];
                return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, 0, nullptr, 0);

            case req::set_configuration:
                // We only have 1 configuration, just ACK.
                return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, 0, nullptr, 0);

            case req::set_interface:
                // We'll just ACK for now.
                // TODO: This will be important for UAC2, CDC and maybe HID, revisit later.
                return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, 0, nullptr, 0);

            default: {
                auto re = renum<req>::unqualified((req)bRequest);
                log::warn(
                    "[usbip] Unhandled Standard Request: %.*s(0x%02x)\n",
                    (int)re.size, re.data, bRequest
                );
                break;
            }
        }
    }
    else if (type == 1) { // Class Request (HID, MIDI, CDC, etc.)
        // TODO: Dispatch to specific class handlers based on wIndex (interface)
        log::warn("[usbip] Unhandled Class Request: 0x%02x\n", bRequest);
    }

    // If we get here, it's an unhandled setup or we need to STALL.
    // Return a RET_SUBMIT with status 0 and 0 length to avoid hanging the host,
    // or return a non-zero status to simulate a STALL.
    return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, config.usbip.stall_status_code, nullptr, 0);
}

auto lm::strands::usbip::transmitting_handle_in(
    const lm::usbip::usbip_cmd_submit& cmd, u32 seqnum, u8 ep_addr) -> bool
{
    auto& data = state_data.transmitting;

    // Host is polling an IN endpoint. Two cases:
    //
    // Case A — data is buffered (arrived via bus before this poll):
    //           send it immediately.
    //
    // Case B — no data yet:
    //           park the request. transmitting_flush_inbound_events() will
    //           send RET_SUBMIT when the next bus event arrives.

    u8 ep_idx = ep_addr & 0x7F;
    log::debug("[usbip] Handling IN URB for ep %u\n", ep_idx);

    if (data.buffered_out[ep_idx].ready) {
        log::debug("[usbip] Case A\n");

        // Case A: send buffered data.
        if (!send_ret_submit(
            seqnum, ep_idx, lm::usbip::USBIP_DIR_IN,
            0, data.buffered_out[ep_idx].data, data.buffered_out[ep_idx].len
        )) return false;
        data.buffered_out[ep_idx] = {};
    } else {
        log::debug("[usbip] Case B\n");

        // Case B: park the request.
        data.pending_in[ep_idx] = {
            .seqnum  = seqnum,
            .max_len = (u16)ntoh32(cmd.transfer_buffer_length),
            .parked  = true,
        };
    }
    return true;
}

auto lm::strands::usbip::transmitting_handle_out(
    const lm::usbip::usbip_cmd_submit& cmd, u32 seqnum, u8 ep_addr) -> bool
{
    log::debug("[usbip] Handling OUT URB\n");

    // Host sent data to us. Read it from the socket, parse it based on
    // which endpoint it arrived on, and publish to the bus.
    //
    // No parking needed — OUT always has data immediately following the header.

    const u32 xfer_len = ntoh32(cmd.transfer_buffer_length);
    u8 payload[512] = {};

    if (xfer_len > 0 && xfer_len <= sizeof(payload)) {
        if (!chip::net::recv_exact(conn_sock, payload, xfer_len)) return false;
    } else if (xfer_len > sizeof(payload)) {
        // Drain oversized transfer
        u8 drain[64];
        u32 remaining = xfer_len;
        while (remaining > 0) {
            u32 chunk = remaining < sizeof(drain) ? remaining : sizeof(drain);
            if (!chip::net::recv_exact(conn_sock, drain, chunk)) return false;
            remaining -= chunk;
        }
    }

    // Publish to the bus based on which endpoint received data.
    // The exact event construction is TBD / handwavy:
    //
    //   MIDI bulk OUT endpoint → publish topic::midi event
    //     payload is 4-byte USB MIDI packets; extract cable/status/d1/d2
    //
    //   HID OUT endpoint → publish topic::hid event
    //     payload is raw report bytes; wrap as-is
    //
    //   CDC bulk OUT endpoint → publish topic::cdc_data event
    //     payload is raw byte stream
    //
    // TODO: map ep_addr → topic based on our descriptor knowledge.
    // Something like:
    //
    //   if (ep_addr == our_midi_bulk_out_ep) {
    //       for each 4-byte USB MIDI packet in payload:
    //           publish(event{topic::midi,...}.with_payload({cable, status, d1, d2}))
    //   }

    // ACK the OUT transfer
    return send_ret_submit(seqnum, ep_addr & 0x7F, lm::usbip::USBIP_DIR_OUT, 0, nullptr, 0);
}


auto lm::strands::usbip::setup_handle_get_descriptor(const u8 setup[8], u32 seqnum) -> bool
{
    const u8 desc_type  = setup[3];
    const u8 desc_index = setup[2];
    const u16 lang_id   = (u16)setup[4] | ((u16)setup[5] << 8); // wIndex (Language ID)
    const u16 req_len   = (u16)setup[6] | ((u16)setup[7] << 8); // wLength, LE

    switch (desc_type) {
        case 0x01: { // DEVICE
            auto len = clamp(sizeof(device_descriptor), 0, req_len);
            log::debug("[usbip] Sending Device descriptor (len=%u/%u)\n", len, sizeof(device_descriptor));
            return send_ret_submit(
                seqnum, 0, lm::usbip::USBIP_DIR_IN, 0,
                (u8 const*)&device_descriptor, len
            );
        }

        case 0x02: { // CONFIGURATION
            auto len = clamp(config_descriptor_size, 0, req_len);
            log::debug("[usbip] Sending Configuration descriptor (len=%u/%u)\n", len, config_descriptor_size);
            return send_ret_submit(
                seqnum, 0, lm::usbip::USBIP_DIR_IN, 0,
                config_descriptor, len
            );
        }

        case 0x03: { // STRING
            // Special Case: Return the Language ID table
            if (desc_index == 0) {
                // This is typically: [bLength=4, bDescriptorType=3, langID_low, langID_high]
                static const u8 lang_table[] = { 4, 0x03, 0x09, 0x04 }; // 0x0409 (English US)
                u16 send_len = req_len < sizeof(lang_table) ? req_len : (u16)sizeof(lang_table);
                return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, 0, lang_table, send_len);
            }

            // Normal Case: Get string by index
            if (desc_index < string_descriptors.size()) {
                const char* src = string_descriptors[desc_index];
                if(desc_index < usb::string_descriptor::count){
                    auto desc_name = renum<usb::string_descriptor::idx>::unqualified(desc_index | toe);
                    log::debug("[usbip] Sending String descriptor (%.*s): [%s])\n", (int)desc_name.size, desc_name.data, src);
                } else{
                    log::debug("[usbip] Sending String descriptor (%u): [%s])\n", desc_index, src);
                }

                // Prepare a temporary buffer for the USB descriptor
                // Format: [Length] [Type=3] [Char0_L] [Char0_H] ...
                u8 usb_packet[config_t::usbcommon::string_descriptor_max_len * 2 + 2];

                u8 char_count = 0;
                while (src[char_count] != '\0' && char_count < config_t::usbcommon::string_descriptor_max_len) {
                    // Fill UTF-16LE (High byte is 0 for standard ASCII)
                    usb_packet[2 + (char_count * 2)]     = (u8)src[char_count];
                    usb_packet[2 + (char_count * 2) + 1] = 0;
                    char_count++;
                }

                usb_packet[0] = (char_count * 2) + 2; // Total Length
                usb_packet[1] = 0x03;                 // Descriptor Type: STRING

                u16 actual_len = usb_packet[0];
                return send_ret_submit(
                    seqnum, 0, lm::usbip::USBIP_DIR_IN, 0,
                    usb_packet, req_len < actual_len ? req_len : actual_len
                );
            }

            // String not found: STALL
            return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, config.usbip.stall_status_code, nullptr, 0);
        }

        default: {
            // STALL — descriptor type not supported
            return send_ret_submit(seqnum, 0, lm::usbip::USBIP_DIR_IN, config.usbip.stall_status_code, nullptr, 0);
        }
    }
}


// Build and send a RET_SUBMIT. Called from both the flush and URB handlers.
// For IN transfers: data/data_len contain the response payload.
// For OUT/STATUS:  data=nullptr, data_len=0.
auto lm::strands::usbip::send_ret_submit(
    u32 seqnum, u32 ep_num, u32 direction,
    u32 status, const void* data, u32 data_len
) -> bool
{
    lm::usbip::usbip_ret_submit ret{};
    ret.header.command    = (lm::usbip::USBIP_URB_Command)hton32(lm::usbip::USBIP_RET_SUBMIT);
    ret.header.seqnum     = hton32(seqnum);
    ret.header.devid      = 0; // spec: server response sets devid to 0
    ret.header.direction  = (lm::usbip::USBIP_Direction)hton32(direction);
    ret.header.ep         = hton32(ep_num);
    ret.status            = hton32(status);
    ret.actual_length     = hton32(data_len);
    ret.start_frame       = 0;
    ret.number_of_packets = hton32(0xFFFFFFFFu); // not isochronous
    ret.error_count       = 0;
    std::memset(ret.padding, 0, sizeof(ret.padding)); // spec mandates zeroed padding

    if (!chip::net::send_exact(conn_sock, &ret, sizeof(ret))) return false;
    if (data && data_len > 0)
        if (!chip::net::send_exact(conn_sock, data, data_len)) return false;
    return true;
}

auto lm::strands::usbip::serialise_event_to_usb(
    const fabric::event& e, u8* out_buf, u8* ep_addr_out) -> u16
{
    auto& data = state_data.transmitting;

    fabric::event dummy;
    // Drain the extensions for now, we might actually want to do something with them later.
    for(auto i = 0; i < e.extension_count(); ++i) data.out_event_q.receive(&dummy, 0);

    // Handwavy — exact event layout TBD.
    // The idea: convert a bus event into the raw bytes that would appear in
    // a USB transfer payload, and tell us which IN endpoint to use.
    //
    // For MIDI (USB MIDI 1.0, 4-byte packets):
    //   if (e.topic == topic::midi) {
    //       auto& m = e.get_payload<midi::note_event>();
    //       out_buf[0] = (m.cable << 4) | cable_index_number(m.status);
    //       out_buf[1] = m.status;
    //       out_buf[2] = m.d1;
    //       out_buf[3] = m.d2;
    //       *ep_addr_out = our_midi_in_ep;
    //       return 4;
    //   }
    //
    // For HID:
    //   if (e.topic == topic::hid) {
    //       auto& r = e.get_payload<hid::report_event>();
    //       std::memcpy(out_buf, r.data, r.len);
    //       *ep_addr_out = our_hid_ep;
    //       return r.len;
    //   }
    //
    // Return 0 for events that have no USB representation on this transport.

    (void)e; (void)out_buf; (void)ep_addr_out;
    return 0; // TODO
}
