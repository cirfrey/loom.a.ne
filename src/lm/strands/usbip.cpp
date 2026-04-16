#include "lm/strands/usbip.hpp"

#include "lm/chip/net.hpp"
#include "lm/core/endian.hpp"
#include "lm/core/cvt.hpp"
#include "lm/board.hpp"
#include "lm/log.hpp"

#include <cstdio>

#include "lm/usb/backend.hpp"
#include "lm/usb/debug.hpp"

namespace lm::strands::usbip_backend
{
    static std::array<
        usb::ep_t,
        config_t::usbip_t::max_endpoints
    > endpoints = {{
        // Only init the first as control, the rest are defalt-initialized as unassigned.
        usb::ep_t{
            .in      = usb::ept_t::control,
            .in_itf  = usb::itf_t::control,
            .out     = usb::ept_t::control,
            .out_itf = usb::itf_t::control
        }
    }};
}

lm::strands::usbip::usbip(fabric::strand_runtime_info&)
{
    state_data.initializing = state_data_t::initializing_t{};

    config.usbip.endpoints = usbip_backend::endpoints;

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
}

auto lm::strands::usbip::on_ready() -> fabric::managed_strand_status
{
    return fabric::managed_strand_status::ok;
}

auto lm::strands::usbip::before_sleep() -> fabric::managed_strand_status
{ return fabric::managed_strand_status::ok; }

// Just a very simple state machine.
auto lm::strands::usbip::on_wake() -> fabric::managed_strand_status
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
            case desired_strand_action::yield: return fabric::managed_strand_status::ok;
            case desired_strand_action::die:   return fabric::managed_strand_status::suicidal;
            default:                           return fabric::managed_strand_status::ok;
        }
    }

    return fabric::managed_strand_status::ok;
}

lm::strands::usbip::~usbip()
{
    if (listen_sock != chip::invalid_socket) chip::net::close(listen_sock);
    if (conn_sock   != chip::invalid_socket) chip::net::close(conn_sock);
}

/// State stuff.

auto lm::strands::usbip::do_initializing_state() -> desired_strand_action
{
    listen_sock = chip::net::make_listen_socket(config.usbip.port);
    if (listen_sock == chip::invalid_socket) {
        log::error("[usbip] Failed to open listen socket on port %i\n", config.usbip.port);
        return desired_strand_action::die;
    }

    chip::net::set_nonblocking(listen_sock);
    log::info("[usbip] Listening on port %i\n", config.usbip.port);
    state = listening;
    state_data.listening = state_data_t::listening_t{};
    return desired_strand_action::loop;
}

auto lm::strands::usbip::do_listening_state() -> desired_strand_action
{
    conn_sock = chip::net::try_accept(listen_sock);
    if (conn_sock == chip::invalid_socket) return desired_strand_action::yield;

    chip::net::set_nodelay(conn_sock);
    log::info("[usbip] Client connected — starting handshake\n");
    state = handshaking;
    state_data.handshaking = state_data_t::handshaking_t{};
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
        chip::net::close(conn_sock);
        conn_sock = chip::invalid_socket;
        state = listening;
        state_data.listening = state_data_t::listening_t{};
        return desired_strand_action::loop;
    }

    if(data.status == data.want_to_go_forward_to_exported)
    {
        state = exported;
        state_data.exported = state_data_t::exported_t{};
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
        log::error("Error while receiving the HEADER during handshaking\n");
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
        log::error("Unknown URB command 0x%08x\n", command);
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
        log::error("Error while receiving BUSID during handshaking\n");
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
        log::error("Error while sending the DEVLIST_REP_HEADER during handshaking\n");
        data.status = data.devlist_header_send_error;
        return;
    }

    /// Device.
    // auto dev = (const tusb_desc_device_t*)tud_descriptor_device_cb();
    // auto cfg_desc = tud_descriptor_configuration_cb(0);
    // auto dev_info = lm::usbip::usbip_device_info{
    //     .path                = {'\0'}, // snprintf later.
    //     .busid               = {'\0'}, // snprintf later.
    //     .busnum              = hton32(1),
    //     .devnum              = hton32(1),
    //     .speed               = (lm::usbip::USBIP_Speed)hton32(lm::usbip::USBIP_SPEED_FULL),
    //     .idVendor            = hton16(dev->idVendor),
    //     .idProduct           = hton16(dev->idProduct),
    //     .bcdDevice           = hton16(dev->bcdDevice),
    //     .bDeviceClass        = dev->bDeviceClass,
    //     .bDeviceSubClass     = dev->bDeviceSubClass,
    //     .bDeviceProtocol     = dev->bDeviceProtocol,
    //     .bConfigurationValue = 1,
    //     .bNumConfigurations  = dev->bNumConfigurations,
    //     .bNumInterfaces      = cfg_desc[4],
    // };
    // std::snprintf(dev_info.path,  sizeof(dev_info.path),  "/sys/bus/usb/devices/1-1");
    // std::snprintf(dev_info.busid, sizeof(dev_info.busid), "1-1");

    // if (!chip::net::send_exact(conn_sock, &dev_info, sizeof(dev_info)))
    // {
    //     log::error("Error while sending the DEVLIST_REP_DEVICEINFO during handshaking\n");
    //     data.status = data.devlist_device_send_error;
    //     return;
    // }

    // /// Interfaces.
    // const u8* p   = cfg_desc;
    // const u8* end = p + ((u16)cfg_desc[2] | ((u16)cfg_desc[3] << 8));
    // p += cfg_desc[0]; // skip config descriptor itself
    // u8 itf_count = 0;
    // while (p < end && itf_count < dev_info.bNumInterfaces) {
    //     if (p[1] != TUSB_DESC_INTERFACE)
    //     {
    //         p += p[0];
    //         continue;
    //     }

    //     lm::usbip::usbip_interface itf_info{};
    //     itf_info.bInterfaceClass    = p[5];
    //     itf_info.bInterfaceSubClass = p[6];
    //     itf_info.bInterfaceProtocol = p[7];
    //     itf_info.padding            = 0;
    //     if (!chip::net::send_exact(conn_sock, &itf_info, sizeof(itf_info)))
    //     {
    //         log::error("Error while sending the DEVLIST_REP_INTERFACE during handshaking\n");
    //         data.status = data.devlist_interface_send_error;
    //         return;
    //     }
    //     ++itf_count;
    //     p += p[0]; // advance by bLength
    // }

    // Now we hopefully wait for OP_REQ_IMPORT, but we need to close the socket first.
    data.status = data.want_to_go_back_to_listening;
}


// Pretty similar to OP_REQ_DEVLIST, we just skip the interfaces and have a slighly
// different header.
auto lm::strands::usbip::handshaking_process_req_import() -> void
{
    auto& data = state_data.handshaking;

    // auto dev = (const tusb_desc_device_t*)tud_descriptor_device_cb();
    // auto cfg_desc = tud_descriptor_configuration_cb(0);
    // auto pkt = lm::usbip::usbip_rep_import{
    //     .version  = hton16(lm::usbip::USBIP_VERSION | tou),
    //     .command  = hton16(lm::usbip::OP_REP_IMPORT | tou),
    //     .status   = 0, // OK
    //     .device = {
    //         .path                = {'\0'}, // snprintf later.
    //         .busid               = {'\0'}, // snprintf later.
    //         .busnum              = hton32(1),
    //         .devnum              = hton32(1),
    //         .speed               = (lm::usbip::USBIP_Speed)hton32(lm::usbip::USBIP_SPEED_FULL),
    //         .idVendor            = hton16(dev->idVendor),
    //         .idProduct           = hton16(dev->idProduct),
    //         .bcdDevice           = hton16(dev->bcdDevice),
    //         .bDeviceClass        = dev->bDeviceClass,
    //         .bDeviceSubClass     = dev->bDeviceSubClass,
    //         .bDeviceProtocol     = dev->bDeviceProtocol,
    //         .bConfigurationValue = 1,
    //         .bNumConfigurations  = dev->bNumConfigurations,
    //         .bNumInterfaces      = cfg_desc[4],
    //     }
    // };
    // std::snprintf(pkt.device.path,  sizeof(pkt.device.path),  "/sys/bus/usb/devices/1-1");
    // std::snprintf(pkt.device.busid, sizeof(pkt.device.busid), "1-1");

    // if (!chip::net::send_exact(conn_sock, &pkt, sizeof(pkt)))
    // {
    //     log::error("Error while sending the DEVLIST_REP_IMPORT during handshaking\n");
    //     data.status = data.import_send_error;
    //     return;
    // }

    // data.status = data.want_to_go_forward_to_exported;
}

auto lm::strands::usbip::do_exported_state() -> desired_strand_action
{
    // OP_REP_IMPORT has been sent — the Linux kernel now thinks we are a
    // USB device. Bind the DCD so TinyUSB starts processing events.
    // bind_port() fires dcd_event_bus_reset() which kicks TinyUSB into
    // its enumeration sequence (GET_DESCRIPTOR etc.), driven by the URBs
    // that start arriving on conn_sock.
    state = transmitting;
    state_data.transmitting = state_data_t::transmitting_t{};

    return desired_strand_action::loop;
}

auto lm::strands::usbip::do_transmitting_state() -> desired_strand_action
{
    // TODO: non-blocking URB poll — read CMD_SUBMIT from conn_sock,
    // dispatch to TinyUSB, send RET_SUBMIT back.
    return desired_strand_action::yield;
}
