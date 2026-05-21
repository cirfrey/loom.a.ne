#pragma once

#include "lm/core/types.hpp"

#include <span>
#include <array>

// NOTE: In USB-speak: IN always means "Into the Computer."
namespace lm::usb
{
    enum class ept_t : u8
    {
        unassigned, // Mark this port/direction as currently unused (free to be used).
        unavailable, // Mark this port/direction as not assignable.

        control, // Used for enumeration and control. Should always be endpoint 0, does not use the endpoint budget.

        // All 3 of these need to be registered for the CDC class, even if you
        // only plan on logging or just receiving data.
        cdc_interrupt_in, //  A "Management" endpoint used to notify the computer about line states (like DTR/RTS) or ring signals. [Device -> Host]
        cdc_bulk_in,      // Send data to PC (logging and stuff). [Device -> Host]
        cdc_bulk_out,     // Receive data from PC. [Host -> Device]

        hid_interrupt_in,  // Input reports endpoint, the one we use to send mouse/keyboard/gamepad events. [Device -> Host].
        hid_interrupt_out, // TLDR: We never use this, favor using the control endpoint instead. [Host -> Device].
                            /// Google AI:
                            /// This is rarely used for simple mice or keyboards, but common for gaming controllers or specialized hardware.
                            /// What it does: It sends "Output Reports" from the computer to your ESP32. Common uses include:
                            /// - Keyboard LEDs: Turning on the Caps Lock or Num Lock light.
                            /// - Force Feedback/Rumble: Telling a controller to vibrate.
                            /// - RGB Control: Changing colors on a gaming peripheral.
                            /// Here is a trick for your "Endpoint Budget": HID allows the host to send Output Reports via the Control Endpoint (Endpoint 0) using a SET_REPORT request.
                            /// - The Benefit: If your device only needs to receive occasional small data (like an LED state), you can skip the dedicated Interrupt OUT endpoint entirely.

        uac2_iso_in,  // Speaker connection.    [Host -> Device].
        uac2_iso_out, // Microphone connection. [Device -> Host].
        uac2_feedback, // Device -> Host.
                        /// Google AI:
                        /// This is a tiny packet sent back to the computer to synchronize the clock. It tells the PC to speed up or slow down the audio stream to prevent "pops" or "clicks."

        // Each midi endpoint has 16 virtual cables/channels.
        midi_bulk_in,  // Send notes to PC. [Device -> Host].
        midi_bulk_out, // Receive notes from PC. [Host -> Device].

        msc_bulk_in,  // [Device -> Host] For data/status
        msc_bulk_out, // [Host -> Device] For data/commands
    };

    enum class itf_t : u8
    {
        unassigned = 0,
        control = 1,
        cdc_comm = 2,
        cdc_data = 3,
        hid = 4,
        uac2_control = 5,
        uac2_streaming = 6,
        midi_control = uac2_control, /// Google AI:
                                     /// Standard USB-MIDI is actually a subclass of USB Audio, so it requires a "dummy" Audio Control interface (often with no endpoints).
        midi_streaming = 7,
        msc = 8,
    };

    struct ep
    {
        ept_t ep : 4 = ept_t::unassigned;
        itf_t itf : 4 = itf_t::unassigned;
        u8 itf_idx = 0;
    };

    struct ep_t
    {
        ept_t in : 4     = ept_t::unassigned;
        u8 in_itf_idx = 0; // What interface is using this endpoint.
        itf_t in_itf  = itf_t::unassigned;

        ept_t out  : 4    = ept_t::unassigned;
        u8 out_itf_idx = 0;
        itf_t out_itf  = itf_t::unassigned;
    };

    struct configuration_descriptor_builder_state_t
    {
        u8* const desc; // Points to the beginning of the descriptor.
        u8 lowest_free_itf_idx;
        u16 desc_curr_len;
        const u16 desc_max_len;

        [[nodiscard]] auto append_desc(u8 const* data, st size) -> st;

        template <u8 DataSize>
        [[nodiscard]] auto append_desc(u8 const (&data)[DataSize]) -> st
        { return append_desc(data, DataSize); }
    };

    // What idx each string descriptor should have.
    // This is reused across backends.
    namespace string_descriptor { enum idx : u8 {
        lang,
        manufacturer,
        product,
        serial,
        midi,
        hid,
        uac,
        cdc,
        msc,

        midi_cable_1,
        midi_cable_start = midi_cable_1, // We do it inverted (this instead of midi_cable_1 = midi_cable_start), this way we get nicer reflection.
        midi_cable_2,
        midi_cable_3,
        midi_cable_4,
        midi_cable_5,
        midi_cable_6,
        midi_cable_7,
        midi_cable_8,
        midi_cable_9,
        midi_cable_10,
        midi_cable_11,
        midi_cable_12,
        midi_cable_13,
        midi_cable_14,
        midi_cable_15,
        midi_cable_16,
        midi_cable_end = midi_cable_16, // Same as midi_cable_start.

        count
    }; }

    // When ep direction is IN we need to do EP_DIR_IN | ep_idx in the configuration descriptor.
    constexpr auto EP_DIR_IN = 0x80_u8;

    struct find_unassigned_ep_ret_t { u8 idx = 0; ep_t* ep = nullptr; };
    constexpr auto find_unassigned_ep_in(std::span<ep_t> eps)
    {
        using ret_t = find_unassigned_ep_ret_t;

        for(u8 i = 1; i < eps.size(); ++i)
            if(eps[i].in == ept_t::unassigned)
                return ret_t{ .idx = i, .ep = eps.data() + i };
        return ret_t {};
    };
    constexpr auto find_unassigned_ep_out(std::span<ep_t> eps)
    {
        using ret_t = find_unassigned_ep_ret_t;

        for(u8 i = 1; i < eps.size(); ++i)
            if(eps[i].out == ept_t::unassigned)
                return ret_t{ .idx = i, .ep = eps.data() + i };
        return ret_t {};
    };
    constexpr auto find_unassigned_ep_inout(std::span<ep_t> eps)
    {
        using ret_t = find_unassigned_ep_ret_t;

        for(u8 i = 1; i < eps.size(); ++i)
            if(eps[i].in == ept_t::unassigned && eps[i].out == ept_t::unassigned)
                return ret_t{ .idx = i, .ep = eps.data() + i };
        return ret_t {};
    };

}

// Thingies to make low-level usb bitbangin' not insufferable.
namespace lm::usb
{
    #pragma pack(push, 1)

    // USB standard utilizes Microsoft Windows Language IDs (LANGID).
    // Used primarily in wIndex of GetDescriptor(String).
    enum class lang_id : u16
    {
        // --- English ---
        english_us          = 0x0409,
        english_uk          = 0x0809,
        english_aus         = 0x0C09,
        english_can         = 0x1009,

        // --- European ---
        spanish_spain       = 0x0C0A,
        spanish_mexico      = 0x080A,
        french_france       = 0x040C,
        french_can          = 0x0C0C,
        german_germany      = 0x0407,
        italian_italy       = 0x0410,
        portuguese_portugal = 0x0816,
        portuguese_brazil   = 0x0416,
        dutch_netherlands   = 0x0413,
        russian             = 0x0419,
        swedish             = 0x041D,

        // --- Asian ---
        japanese            = 0x0411,
        korean              = 0x0412,
        chinese_traditional = 0x0404, // Taiwan
        chinese_simplified  = 0x0804, // PRC

        // --- Special ---
        unicode_default     = 0x0000  // Sometimes used as a fallback
    };

    namespace setup_request_ns
    {
        enum class direction : u8
        {
            host_to_device = 0x00,
            device_to_host = 0x01,
        };

        enum class category : u8
        {
            standard       = 0x00,
            class_specific = 0x01,
            vendor         = 0x02,
            reserved       = 0x03,
        };

        enum class recipient : u8
        {
            device         = 0x00,
            interface      = 0x01,
            endpoint       = 0x02,
            other          = 0x03,
        };

        struct type {
            u8 raw;
            constexpr auto dir() const -> direction { return static_cast<direction>((raw >> 7) & 0x01); }
            constexpr auto cat() const -> category  { return static_cast<category>((raw >> 5) & 0x03); }
            constexpr auto rec() const -> recipient { return static_cast<recipient>(raw & 0x1F); }
            constexpr auto set(direction d) -> type& { raw = (raw & 0x7F) | (static_cast<u8>(d) << 7); return *this; }
            constexpr auto set(category c)  -> type& { raw = (raw & 0x9F) | (static_cast<u8>(c) << 5); return *this; }
            constexpr auto set(recipient r) -> type& { raw = (raw & 0xE0) | (static_cast<u8>(r) & 0x1F); return *this; }
        };

        // Reference table of raw bRequest values across all USB classes.
        // Values from different classes intentionally share the same underlying byte — that is
        // spec-defined and expected. Use the class-specific req enums (standard::req, cdc::req, etc.)
        // for type-safe handling; use this only as a lookup reference.
        enum class class_req : u8
        {
            // ====================================================================
            // STANDARD USB 2.0 / 3.x REQUESTS (Ch. 9)
            // ====================================================================
            std_get_status        = 0x00,
            std_clear_feature     = 0x01,
            std_set_feature       = 0x03,
            std_set_address       = 0x05,
            std_get_descriptor    = 0x06,
            std_set_descriptor    = 0x07,
            std_get_configuration = 0x08,
            std_set_configuration = 0x09,
            std_get_interface     = 0x0A,
            std_set_interface     = 0x0B,
            std_synch_frame       = 0x0C,
            std_set_sel           = 0x30, // USB 3.0: Set System Exit Latency
            std_set_isoch_delay   = 0x31, // USB 3.0: Set Isochronous Delay

            // ====================================================================
            // CDC
            // ====================================================================
            cdc_send_encapsulated_cmd  = 0x00,
            cdc_get_encapsulated_rsp   = 0x01,
            cdc_set_comm_feature       = 0x02,
            cdc_get_comm_feature       = 0x03,
            cdc_clear_comm_feature     = 0x04,
            cdc_set_aux_line_trigger   = 0x10,
            cdc_set_hook_state         = 0x11,
            cdc_get_hook_state         = 0x12,
            cdc_clear_pulse            = 0x13,
            cdc_send_pulse             = 0x14,
            cdc_set_line_coding        = 0x20,
            cdc_get_line_coding        = 0x21,
            cdc_set_control_line_state = 0x22,
            cdc_send_break             = 0x23,
            cdc_set_reassigned_ringer  = 0x24,
            cdc_get_reassigned_ringer  = 0x25,
            cdc_set_msc_line_coding    = 0x26,
            cdc_get_msc_line_coding    = 0x27,
            cdc_set_msc_control_line   = 0x28,
            cdc_get_msc_status         = 0x29,
            cdc_set_pulse_dial_mode    = 0x2A,
            cdc_send_steps_to_dial     = 0x2B,
            cdc_set_auto_dial_order    = 0x2C,
            cdc_get_auto_dial_status   = 0x2D,
            cdc_set_ringer_parms       = 0x30, // Telephone Control Model
            cdc_get_ringer_parms       = 0x31,
            cdc_set_operation_parms    = 0x32,
            cdc_get_operation_parms    = 0x33,
            cdc_set_line_parms         = 0x34,
            cdc_get_line_parms         = 0x35,
            cdc_dial_digits            = 0x36,
            cdc_set_eth_multicast_fltr = 0x40,
            cdc_set_eth_pm_patt_fltr   = 0x41,
            cdc_get_eth_pm_patt_fltr   = 0x42,
            cdc_set_eth_packet_filter  = 0x43,
            cdc_get_eth_statistic      = 0x44,
            cdc_set_atm_data_format    = 0x50,
            cdc_get_atm_device_status  = 0x51,
            cdc_set_atm_default_vc     = 0x52,
            cdc_get_atm_vc_statistics  = 0x53,

            // ====================================================================
            // HID
            // ====================================================================
            hid_get_report             = 0x01,
            hid_get_idle               = 0x02,
            hid_get_protocol           = 0x03,
            hid_set_report             = 0x09,
            hid_set_idle               = 0x0A,
            hid_set_protocol           = 0x0B,

            // ====================================================================
            // MSC
            // ====================================================================
            msc_get_max_lun            = 0xFE,
            msc_bulk_only_reset        = 0xFF,

            // ====================================================================
            // DFU v1.1
            // ====================================================================
            dfu_detach                 = 0x00,
            dfu_download               = 0x01,
            dfu_upload                 = 0x02,
            dfu_get_status             = 0x03,
            dfu_clear_status           = 0x04,
            dfu_get_state              = 0x05,
            dfu_abort                  = 0x06,

            // ====================================================================
            // UAC1 / UAC2 & MIDI
            // Note: UAC1 embeds direction in bRequest (e.g. get_cur = 0x81).
            // ====================================================================
            audio_v1_set_cur           = 0x01,
            audio_v1_get_cur           = 0x81,
            audio_v1_set_min           = 0x02,
            audio_v1_get_min           = 0x82,
            audio_v1_set_max           = 0x03,
            audio_v1_get_max           = 0x83,
            audio_v1_set_res           = 0x04,
            audio_v1_get_res           = 0x84,
            audio_v1_set_mem           = 0x05,
            audio_v1_get_mem           = 0x85,
            audio_v1_get_stat          = 0xFF,
            audio_v2_curr              = 0x01,
            audio_v2_range             = 0x02,
            audio_v2_mem               = 0x03,

            // ====================================================================
            // HUB
            // ====================================================================
            hub_get_descriptor         = 0x06,
            hub_clear_tt_buffer        = 0x08,
            hub_reset_tt               = 0x09,
            hub_get_tt_state           = 0x0A,
            hub_stop_tt                = 0x0B,

            // ====================================================================
            // WEBUSB / VENDOR
            // ====================================================================
            webusb_get_url             = 0x02,
            ms_os_20_descriptor        = 0x07,
        };
    }

    // ============================================================================
    // STANDARD (Chapter 9)
    // ============================================================================
    namespace setup_request_ns::standard
    {
        enum class req : u8
        {
            get_status        = (u8)class_req::std_get_status,
            clear_feature     = (u8)class_req::std_clear_feature,
            set_feature       = (u8)class_req::std_set_feature,
            set_address       = (u8)class_req::std_set_address,
            get_descriptor    = (u8)class_req::std_get_descriptor,
            set_descriptor    = (u8)class_req::std_set_descriptor,
            get_configuration = (u8)class_req::std_get_configuration,
            set_configuration = (u8)class_req::std_set_configuration,
            get_interface     = (u8)class_req::std_get_interface,
            set_interface     = (u8)class_req::std_set_interface,
            synch_frame       = (u8)class_req::std_synch_frame,
            set_sel           = (u8)class_req::std_set_sel,
            set_isoch_delay   = (u8)class_req::std_set_isoch_delay,
        };

        enum class desc : u8
        {
            device            = 0x01,
            configuration     = 0x02,
            string            = 0x03,
            interface         = 0x04,
            endpoint          = 0x05,
            device_qualifier  = 0x06,
            other_speed       = 0x07,
            interface_power   = 0x08,
            otg               = 0x09,
            debug             = 0x0A,
            interface_assoc   = 0x0B,
            bos               = 0x0F,
            device_capability = 0x10,

            // These annoying little bastards are class specific descriptors that still use the standard GetDescriptor request.
            // The descriptor type is just a made-up value that the class spec decided to use to differentiate from the normal descriptors.
            hid = 0x21, // The main functional block detailing how many extra class descriptors follow.
            report = 0x22, // The physical layout maps for keys, mice, or gamepads.
            physical = 0x23, // Rarely used descriptor for physical human interface traits.
            hub = 0x29, // USB 2.0 Hub descriptor, used to define port counts and power switching.
            superspeed_hub = 0x2A, // USB 3.0 version of the Hub descriptor.
        };

        enum class feat : u16
        {
            endpoint_halt        = 0x0000,
            device_remote_wakeup = 0x0001,
            test_mode            = 0x0002,
        };

        struct descriptor_fields_t {
            type bmRequestType;
            req  bRequest;
            u8   index;
            desc descriptor_type;
            u16  wIndex;
            u16  wLength;
        };
        struct string_descriptor_fields_t {
            type      bmRequestType;
            req       bRequest;
            u8        index;           // Maps to low byte of wValue
            desc      descriptor_type; // Maps to high byte of wValue (Must be desc::string)
            lang_id   language_id;     // Maps to wIndex.
            u16       wLength;
        };
        struct interface_descriptor_fields_t {
            type      bmRequestType;
            req       bRequest;
            u8        index;           // Maps to low byte of wValue
            desc      descriptor_type; // Maps to high byte of wValue (e.g., desc::report)
            u16       interface_number;// Maps to wIndex.
            u16       wLength;
        };
        struct feature_fields_t {
            type bmRequestType;
            req  bRequest;
            feat selector;
            u16  wIndex;
            u16  wLength;
        };
        struct address_fields_t {
            type bmRequestType;
            req  bRequest;
            u8   address;
            u8   _reserved0 = 0;
            u16  wIndex;
            u16  wLength;
        };
        struct configuration_fields_t {
            type bmRequestType;
            req  bRequest;
            u8   configuration_value;
            u8   _reserved0 = 0;
            u16  wIndex;
            u16  wLength;
        };
        struct interface_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  alternate_setting;
            u16  interface_number;
            u16  wLength;
        };
        struct status_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;     // must be 0
            u16  wIndex;
            u16  wLength;    // must be 2
        };
        struct synch_frame_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;     // must be 0; frame number is returned in the DATA stage
            u16  wIndex;     // endpoint address
            u16  wLength;    // 2
        };
        struct sel_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;     // zero
            u16  wIndex;     // zero
            u16  wLength;    // 6 — SEL data payload: U1SEL(1) U1PEL(1) U2SEL(2) U2PEL(2)
        };
        struct isoch_delay_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  delay_ns;   // wValue: isochronous delay in nanoseconds
            u16  wIndex;     // zero
            u16  wLength;    // zero
        };
    }

    // ============================================================================
    // CDC
    // ============================================================================
    namespace setup_request_ns::cdc
    {
        enum class req : u8
        {
            send_encapsulated_cmd  = (u8)class_req::cdc_send_encapsulated_cmd,
            get_encapsulated_rsp   = (u8)class_req::cdc_get_encapsulated_rsp,
            set_comm_feature       = (u8)class_req::cdc_set_comm_feature,
            get_comm_feature       = (u8)class_req::cdc_get_comm_feature,
            clear_comm_feature     = (u8)class_req::cdc_clear_comm_feature,
            set_aux_line_trigger   = (u8)class_req::cdc_set_aux_line_trigger,
            set_hook_state         = (u8)class_req::cdc_set_hook_state,
            get_hook_state         = (u8)class_req::cdc_get_hook_state,
            clear_pulse            = (u8)class_req::cdc_clear_pulse,
            send_pulse             = (u8)class_req::cdc_send_pulse,
            set_line_coding        = (u8)class_req::cdc_set_line_coding,
            get_line_coding        = (u8)class_req::cdc_get_line_coding,
            set_control_line_state = (u8)class_req::cdc_set_control_line_state,
            send_break             = (u8)class_req::cdc_send_break,
            set_reassigned_ringer  = (u8)class_req::cdc_set_reassigned_ringer,
            get_reassigned_ringer  = (u8)class_req::cdc_get_reassigned_ringer,
            set_msc_line_coding    = (u8)class_req::cdc_set_msc_line_coding,
            get_msc_line_coding    = (u8)class_req::cdc_get_msc_line_coding,
            set_msc_control_line   = (u8)class_req::cdc_set_msc_control_line,
            get_msc_status         = (u8)class_req::cdc_get_msc_status,
            set_pulse_dial_mode    = (u8)class_req::cdc_set_pulse_dial_mode,
            send_steps_to_dial     = (u8)class_req::cdc_send_steps_to_dial,
            set_auto_dial_order    = (u8)class_req::cdc_set_auto_dial_order,
            get_auto_dial_status   = (u8)class_req::cdc_get_auto_dial_status,
            set_ringer_parms       = (u8)class_req::cdc_set_ringer_parms,
            get_ringer_parms       = (u8)class_req::cdc_get_ringer_parms,
            set_operation_parms    = (u8)class_req::cdc_set_operation_parms,
            get_operation_parms    = (u8)class_req::cdc_get_operation_parms,
            set_line_parms         = (u8)class_req::cdc_set_line_parms,
            get_line_parms         = (u8)class_req::cdc_get_line_parms,
            dial_digits            = (u8)class_req::cdc_dial_digits,
            set_eth_multicast_fltr = (u8)class_req::cdc_set_eth_multicast_fltr,
            set_eth_pm_patt_fltr   = (u8)class_req::cdc_set_eth_pm_patt_fltr,
            get_eth_pm_patt_fltr   = (u8)class_req::cdc_get_eth_pm_patt_fltr,
            set_eth_packet_filter  = (u8)class_req::cdc_set_eth_packet_filter,
            get_eth_statistic      = (u8)class_req::cdc_get_eth_statistic,
            set_atm_data_format    = (u8)class_req::cdc_set_atm_data_format,
            get_atm_device_status  = (u8)class_req::cdc_get_atm_device_status,
            set_atm_default_vc     = (u8)class_req::cdc_set_atm_default_vc,
            get_atm_vc_statistics  = (u8)class_req::cdc_get_atm_vc_statistics,
        };

        enum class comm_feature : u16
        {
            abstract_state  = 0x0001,
            country_setting = 0x0002,
        };

        // Generic layout: covers encapsulated cmd/rsp, line coding, and all
        // telephone/ethernet/ATM requests where wValue has no distinct semantic fields.
        struct generic_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;
            u16  interface_num;
            u16  wLength;
        };
        // Aliases for union members that share this layout but are semantically distinct.
        using encapsulated_fields_t  = generic_fields_t;
        using line_coding_fields_t   = generic_fields_t;

        struct control_line_state_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  line_state;   // bit 0: DTR (DTE present), bit 1: RTS (activate carrier)
            u16  interface_num;
            u16  wLength;      // zero
        };
        struct send_break_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  duration_ms;  // 0xFFFF = indefinite, 0x0000 = stop break
            u16  interface_num;
            u16  wLength;      // zero
        };
        struct comm_feature_fields_t {
            type         bmRequestType;
            req          bRequest;
            comm_feature feature;   // wValue
            u16          interface_num;
            u16          wLength;   // 2
        };
    }

    // ============================================================================
    // HID
    // ============================================================================
    namespace setup_request_ns::hid
    {
        enum class req : u8
        {
            get_report   = (u8)class_req::hid_get_report,
            get_idle     = (u8)class_req::hid_get_idle,
            get_protocol = (u8)class_req::hid_get_protocol,
            set_report   = (u8)class_req::hid_set_report,
            set_idle     = (u8)class_req::hid_set_idle,
            set_protocol = (u8)class_req::hid_set_protocol,
        };

        enum class report_type : u8
        {
            input   = 0x01,
            output  = 0x02,
            feature = 0x03,
        };

        enum class protocol : u8
        {
            boot   = 0x00,
            report = 0x01,
        };

        struct report_fields_t {
            type        bmRequestType;
            req         bRequest;
            u8          report_id;
            report_type report_type_;   // high byte of wValue
            u16         interface_num;
            u16         wLength;
        };
        struct idle_fields_t {
            type bmRequestType;
            req  bRequest;
            u8   report_id;             // low byte of wValue
            u8   duration;              // high byte of wValue, in 4ms units; 0 = indefinite
            u16  interface_num;
            u16  wLength;               // 1 for GET, 0 for SET
        };
        struct protocol_fields_t {
            type     bmRequestType;
            req      bRequest;
            protocol protocol_;
            u8       _pad = 0;          // high byte of wValue, must be zero
            u16      interface_num;
            u16      wLength;           // 1 for GET, 0 for SET
        };
    }

    // ============================================================================
    // MSC
    // ============================================================================
    namespace setup_request_ns::msc
    {
        enum class req : u8
        {
            get_max_lun  = (u8)class_req::msc_get_max_lun,
            reset        = (u8)class_req::msc_bulk_only_reset,
        };

        struct get_max_lun_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;        // zero
            u16  interface_num;
            u16  wLength;       // 1
        };
        struct reset_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;        // zero
            u16  interface_num;
            u16  wLength;       // zero
        };
    }

    // ============================================================================
    // DFU v1.1
    // ============================================================================
    namespace setup_request_ns::dfu
    {
        enum class req : u8
        {
            detach       = (u8)class_req::dfu_detach,
            download     = (u8)class_req::dfu_download,
            upload       = (u8)class_req::dfu_upload,
            get_status   = (u8)class_req::dfu_get_status,
            clear_status = (u8)class_req::dfu_clear_status,
            get_state    = (u8)class_req::dfu_get_state,
            abort        = (u8)class_req::dfu_abort,
        };

        struct detach_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  timeout_ms;    // wDetachTimeout: max ms device waits for USB reset
            u16  interface_num;
            u16  wLength;       // zero
        };
        // Shared by DNLOAD and UPLOAD.
        struct transfer_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  block_num;     // wBlockNum: block sequence number
            u16  interface_num;
            u16  wLength;       // payload length; 0 on final DNLOAD block
        };
        // Shared by GET_STATUS, CLEAR_STATUS, GET_STATE, ABORT.
        struct simple_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;        // zero
            u16  interface_num;
            u16  wLength;       // 6 for GET_STATUS, 1 for GET_STATE, 0 for others
        };
    }

    // ============================================================================
    // AUDIO (UAC1 + UAC2)
    // ============================================================================
    namespace setup_request_ns::audio
    {
        namespace v1
        {
            // Direction is embedded in the request code itself for UAC1.
            enum class req : u8
            {
                set_cur  = (u8)class_req::audio_v1_set_cur,
                get_cur  = (u8)class_req::audio_v1_get_cur,
                set_min  = (u8)class_req::audio_v1_set_min,
                get_min  = (u8)class_req::audio_v1_get_min,
                set_max  = (u8)class_req::audio_v1_set_max,
                get_max  = (u8)class_req::audio_v1_get_max,
                set_res  = (u8)class_req::audio_v1_set_res,
                get_res  = (u8)class_req::audio_v1_get_res,
                set_mem  = (u8)class_req::audio_v1_set_mem,
                get_mem  = (u8)class_req::audio_v1_get_mem,
                get_stat = (u8)class_req::audio_v1_get_stat,
            };

            struct fields_t {
                type bmRequestType;
                req  bRequest;
                u8   channel_num;   // low byte of wValue
                u8   control_sel;   // high byte of wValue (control selector, entity-type specific)
                u8   iface_or_ep;   // low byte of wIndex (interface or endpoint number)
                u8   entity_id;     // high byte of wIndex (unit or terminal ID)
                u16  wLength;
            };
        }

        namespace v2
        {
            // Direction is in bmRequestType for UAC2; bRequest values are simpler.
            enum class req : u8
            {
                cur   = (u8)class_req::audio_v2_curr,
                range = (u8)class_req::audio_v2_range,
                mem   = (u8)class_req::audio_v2_mem,
            };

            struct fields_t {
                type bmRequestType;
                req  bRequest;
                u8   channel_num;   // low byte of wValue
                u8   control_sel;   // high byte of wValue
                u8   iface_or_ep;   // low byte of wIndex
                u8   entity_id;     // high byte of wIndex
                u16  wLength;
            };
        }
    }

    // ============================================================================
    // HUB
    // ============================================================================
    namespace setup_request_ns::hub
    {
        // Hub reuses standard request codes (0x00, 0x01, 0x03) but with
        // class-specific bmRequestType. Class-specific codes for TT management follow.
        enum class req : u8
        {
            get_status      = 0x00,
            clear_feature   = 0x01,
            set_feature     = 0x03,
            get_descriptor  = (u8)class_req::hub_get_descriptor,
            clear_tt_buffer = (u8)class_req::hub_clear_tt_buffer,
            reset_tt        = (u8)class_req::hub_reset_tt,
            get_tt_state    = (u8)class_req::hub_get_tt_state,
            stop_tt         = (u8)class_req::hub_stop_tt,
        };

        enum class hub_feature : u16
        {
            c_hub_local_power  = 0,
            c_hub_over_current = 1,
        };

        enum class port_feature : u16
        {
            port_connection     = 0,
            port_enable         = 1,
            port_suspend        = 2,
            port_over_current   = 3,
            port_reset          = 4,
            port_power          = 8,
            port_low_speed      = 9,
            c_port_connection   = 16,
            c_port_enable       = 17,
            c_port_suspend      = 18,
            c_port_over_current = 19,
            c_port_reset        = 20,
            port_test           = 21,
            port_indicator      = 22,
        };

        struct hub_feature_fields_t {
            type        bmRequestType;
            req         bRequest;
            hub_feature feature;    // wValue
            u16         wIndex;     // zero for hub-level features
            u16         wLength;    // zero
        };
        struct port_feature_fields_t {
            type         bmRequestType;
            req          bRequest;
            port_feature feature;   // wValue
            u8           port_num;  // low byte of wIndex
            u8           selector;  // high byte of wIndex (used for PORT_INDICATOR: 0=auto 1=amber 2=green)
            u16          wLength;   // zero
        };
        struct status_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;    // zero
            u16  wIndex;    // 0 for hub status, port number for port status
            u16  wLength;   // 4 (returns 4-byte wHubStatus + wHubChange)
        };
        struct descriptor_fields_t {
            type bmRequestType;
            req  bRequest;
            u8   index;             // descriptor index (usually 0)
            u8   descriptor_type;   // 0x29 = hub descriptor, 0x2A = SuperSpeed hub descriptor
            u16  wIndex;            // zero
            u16  wLength;
        };
        struct tt_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  dev_ep;    // wValue: for CLEAR_TT_BUFFER — device address in low 7 bits, ep in next 4
            u8   tt_port;   // low byte of wIndex: TT port number
            u8   _reserved = 0;
            u16  wLength;   // zero
        };
    }

    // ============================================================================
    // VENDOR (WebUSB, MS OS 2.0)
    // ============================================================================
    namespace setup_request_ns::vendor
    {
        enum class req : u8
        {
            webusb_get_url      = (u8)class_req::webusb_get_url,
            ms_os_20_descriptor = (u8)class_req::ms_os_20_descriptor,
        };

        struct webusb_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  url_index; // wValue: index into the URL descriptor table
            u16  wIndex;    // 0x0002 = WEBUSB_REQUEST_GET_URL
            u16  wLength;
        };
        struct ms_os_20_fields_t {
            type bmRequestType;
            req  bRequest;
            u16  wValue;    // zero for OS 2.0 descriptor set
            u16  wIndex;    // 0x0007 = MS_OS_20_DESCRIPTOR_INDEX
            u16  wLength;
        };
    }

    // ============================================================================
    // SETUP REQUEST
    // ============================================================================
    struct setup_request
    {
        using direction = setup_request_ns::direction;
        using category  = setup_request_ns::category;
        using recipient = setup_request_ns::recipient;
        using type      = setup_request_ns::type;

        struct fields_t {
            type bmRequestType;
            u8   bRequest;
            u16  wValue;
            u16  wIndex;
            u16  wLength;
        };

        union {
            fields_t fields;

            // Standard
            setup_request_ns::standard::descriptor_fields_t    standard_descriptor_fields;
            setup_request_ns::standard::string_descriptor_fields_t    standard_string_descriptor_fields;
            setup_request_ns::standard::interface_descriptor_fields_t standard_interface_descriptor_fields;
            setup_request_ns::standard::feature_fields_t       standard_feature_fields;
            setup_request_ns::standard::address_fields_t       standard_address_fields;
            setup_request_ns::standard::configuration_fields_t standard_configuration_fields;
            setup_request_ns::standard::interface_fields_t     standard_interface_fields;
            setup_request_ns::standard::status_fields_t        standard_status_fields;
            setup_request_ns::standard::synch_frame_fields_t   standard_synch_frame_fields;
            setup_request_ns::standard::sel_fields_t           standard_sel_fields;
            setup_request_ns::standard::isoch_delay_fields_t   standard_isoch_delay_fields;

            // CDC
            setup_request_ns::cdc::generic_fields_t            cdc_encapsulated_fields;
            setup_request_ns::cdc::generic_fields_t            cdc_line_coding_fields;
            setup_request_ns::cdc::control_line_state_fields_t cdc_control_line_state_fields;
            setup_request_ns::cdc::send_break_fields_t         cdc_send_break_fields;
            setup_request_ns::cdc::comm_feature_fields_t       cdc_comm_feature_fields;

            // HID
            setup_request_ns::hid::report_fields_t             hid_report_fields;
            setup_request_ns::hid::idle_fields_t               hid_idle_fields;
            setup_request_ns::hid::protocol_fields_t           hid_protocol_fields;

            // MSC
            setup_request_ns::msc::get_max_lun_fields_t        msc_get_max_lun_fields;
            setup_request_ns::msc::reset_fields_t              msc_reset_fields;

            // DFU
            setup_request_ns::dfu::detach_fields_t             dfu_detach_fields;
            setup_request_ns::dfu::transfer_fields_t           dfu_transfer_fields;
            setup_request_ns::dfu::simple_fields_t             dfu_simple_fields;

            // Audio
            setup_request_ns::audio::v1::fields_t              audio_v1_fields;
            setup_request_ns::audio::v2::fields_t              audio_v2_fields;

            // Hub
            setup_request_ns::hub::hub_feature_fields_t        hub_hub_feature_fields;
            setup_request_ns::hub::port_feature_fields_t       hub_port_feature_fields;
            setup_request_ns::hub::status_fields_t             hub_status_fields;
            setup_request_ns::hub::descriptor_fields_t         hub_descriptor_fields;
            setup_request_ns::hub::tt_fields_t                 hub_tt_fields;

            // Vendor
            setup_request_ns::vendor::webusb_fields_t          vendor_webusb_fields;
            setup_request_ns::vendor::ms_os_20_fields_t        vendor_ms_os_20_fields;

            std::array<u8, 8> bytes;
            std::array<u8, 6> bytes_short; // No wLength — inferred from the event's extension payload size.
        };

        #define LM_USB_DEFINE_VIEW(FUNCNAME, UNION_NAME, CHECK) \
            static constexpr auto FUNCNAME(setup_request const& sr) -> decltype(setup_request::UNION_NAME) const* { \
                if(CHECK) return &sr.UNION_NAME; \
                return nullptr; \
            } \
            static constexpr auto FUNCNAME(setup_request& sr) -> decltype(setup_request::UNION_NAME)* { \
                if(CHECK) return &sr.UNION_NAME; \
                return nullptr; \
            }

        // ========================================================================
        // STANDARD
        // ========================================================================
        struct standard
        {
            using req  = setup_request_ns::standard::req;
            using desc = setup_request_ns::standard::desc;
            using feat = setup_request_ns::standard::feat;

            struct get_descriptor_params {
                desc      descriptor_type = desc::device;
                u8        index           = 0;
                u16       length          = 0;
                recipient recipient_      = recipient::device;
            };
            static constexpr auto get_descriptor(get_descriptor_params p) -> setup_request {
                return setup_request{ .standard_descriptor_fields = {
                    .bmRequestType   = type{}.set(direction::device_to_host).set(category::standard).set(p.recipient_),
                    .bRequest        = req::get_descriptor,
                    .index           = p.index,
                    .descriptor_type = p.descriptor_type,
                    .wIndex          = 0,
                    .wLength         = p.length,
                }};
            }
            struct get_string_descriptor_params {
                u8        index       = 0;
                u16       length      = 0;
                lang_id   language_id = lang_id::english_us;
            };
            static constexpr auto get_string_descriptor(get_string_descriptor_params p) -> setup_request {
                return setup_request{ .standard_string_descriptor_fields = {
                    .bmRequestType   = type{}.set(direction::device_to_host).set(category::standard).set(recipient::device),
                    .bRequest        = req::get_descriptor,
                    .index           = p.index,
                    .descriptor_type = desc::string,
                    .language_id     = p.language_id,
                    .wLength         = p.length,
                }};
            }
            struct get_interface_descriptor_params {
                desc      descriptor_type;
                u8        index            = 0;
                u16       length           = 0;
                u16       interface_number = 0;
            };
            static constexpr auto get_interface_descriptor(get_interface_descriptor_params p) -> setup_request {
                return setup_request{ .standard_interface_descriptor_fields = {
                    .bmRequestType    = type{}.set(direction::device_to_host).set(category::standard).set(recipient::interface),
                    .bRequest         = req::get_descriptor,
                    .index            = p.index,
                    .descriptor_type  = p.descriptor_type,
                    .interface_number = p.interface_number,
                    .wLength          = p.length,
                }};
            }

            struct set_descriptor_params {
                desc      descriptor_type;
                u8        index       = 0;
                u16       length      = 0;
                u16       wIndex      = 0;
                recipient recipient_  = recipient::device;
            };
            static constexpr auto set_descriptor(set_descriptor_params p) -> setup_request {
                return setup_request{ .standard_descriptor_fields = {
                    .bmRequestType   = type{}.set(direction::host_to_device).set(category::standard).set(p.recipient_),
                    .bRequest        = req::set_descriptor,
                    .index           = p.index,
                    .descriptor_type = p.descriptor_type,
                    .wIndex          = p.wIndex,
                    .wLength         = p.length,
                }};
            }
            struct set_string_descriptor_params {
                u8        index       = 0;
                u16       length      = 0;
                lang_id   language_id = lang_id::english_us;
            };
            static constexpr auto set_string_descriptor(set_string_descriptor_params p) -> setup_request {
                return setup_request{ .standard_string_descriptor_fields = {
                    .bmRequestType   = type{}.set(direction::host_to_device).set(category::standard).set(recipient::device),
                    .bRequest        = req::set_descriptor,
                    .index           = p.index,
                    .descriptor_type = desc::string,
                    .language_id     = p.language_id,
                    .wLength         = p.length,
                }};
            }
            struct set_interface_descriptor_params {
                desc      descriptor_type;
                u8        index            = 0;
                u16       length           = 0;
                u16       interface_number = 0;
            };
            static constexpr auto set_interface_descriptor(set_interface_descriptor_params p) -> setup_request {
                return setup_request{ .standard_interface_descriptor_fields = {
                    .bmRequestType    = type{}.set(direction::host_to_device).set(category::standard).set(recipient::interface),
                    .bRequest         = req::set_descriptor,
                    .index            = p.index,
                    .descriptor_type  = p.descriptor_type,
                    .interface_number = p.interface_number,
                    .wLength          = p.length,
                }};
            }

            struct feature_params {
                feat      selector;
                u16       index    = 0;
                recipient recipient_ = recipient::device;
            };
            static constexpr auto set_feature(feature_params p) -> setup_request {
                return setup_request{ .standard_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::standard).set(p.recipient_),
                    .bRequest      = req::set_feature,
                    .selector      = p.selector,
                    .wIndex        = p.index,
                    .wLength       = 0,
                }};
            }
            static constexpr auto clear_feature(feature_params p) -> setup_request {
                return setup_request{ .standard_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::standard).set(p.recipient_),
                    .bRequest      = req::clear_feature,
                    .selector      = p.selector,
                    .wIndex        = p.index,
                    .wLength       = 0,
                }};
            }

            struct set_address_params { u8 address; };
            static constexpr auto set_address(set_address_params p) -> setup_request {
                return setup_request{ .standard_address_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::standard).set(recipient::device),
                    .bRequest      = req::set_address,
                    .address       = p.address,
                    .wIndex        = 0,
                    .wLength       = 0,
                }};
            }

            struct set_configuration_params { u8 configuration_value; };
            static constexpr auto set_configuration(set_configuration_params p) -> setup_request {
                return setup_request{ .standard_configuration_fields = {
                    .bmRequestType       = type{}.set(direction::host_to_device).set(category::standard).set(recipient::device),
                    .bRequest            = req::set_configuration,
                    .configuration_value = p.configuration_value,
                    .wIndex              = 0,
                    .wLength             = 0,
                }};
            }
            static constexpr auto get_configuration() -> setup_request {
                return setup_request{ .standard_configuration_fields = {
                    .bmRequestType       = type{}.set(direction::device_to_host).set(category::standard).set(recipient::device),
                    .bRequest            = req::get_configuration,
                    .configuration_value = 0,
                    .wIndex              = 0,
                    .wLength             = 1,
                }};
            }

            struct get_interface_params {
                u8        interface_number;
                recipient recipient_ = recipient::interface;
            };
            static constexpr auto get_interface(get_interface_params p) -> setup_request {
                return setup_request{ .standard_interface_fields = {
                    .bmRequestType     = type{}.set(direction::device_to_host).set(category::standard).set(p.recipient_),
                    .bRequest          = req::get_interface,
                    .alternate_setting = 0,
                    .interface_number  = p.interface_number,
                    .wLength           = 1,
                }};
            }

            struct set_interface_params {
                u8        interface_number;
                u8        alternate_setting;
                recipient recipient_ = recipient::interface;
            };
            static constexpr auto set_interface(set_interface_params p) -> setup_request {
                return setup_request{ .standard_interface_fields = {
                    .bmRequestType     = type{}.set(direction::host_to_device).set(category::standard).set(p.recipient_),
                    .bRequest          = req::set_interface,
                    .alternate_setting = p.alternate_setting,
                    .interface_number  = p.interface_number,
                    .wLength           = 0,
                }};
            }

            struct get_status_params {
                recipient recipient_ = recipient::device;
                u16       index      = 0;
            };
            static constexpr auto get_status(get_status_params p) -> setup_request {
                return setup_request{ .standard_status_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::standard).set(p.recipient_),
                    .bRequest      = req::get_status,
                    .wValue        = 0,
                    .wIndex        = p.index,
                    .wLength       = 2,
                }};
            }

            struct synch_frame_params { u16 endpoint_address; };
            static constexpr auto synch_frame(synch_frame_params p) -> setup_request {
                return setup_request{ .standard_synch_frame_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::standard).set(recipient::endpoint),
                    .bRequest      = req::synch_frame,
                    .wValue        = 0,
                    .wIndex        = p.endpoint_address,
                    .wLength       = 2,
                }};
            }

            static constexpr auto set_sel() -> setup_request {
                return setup_request{ .standard_sel_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::standard).set(recipient::device),
                    .bRequest      = req::set_sel,
                    .wValue        = 0,
                    .wIndex        = 0,
                    .wLength       = 6,
                }};
            }

            struct set_isoch_delay_params { u16 delay_ns; };
            static constexpr auto set_isoch_delay(set_isoch_delay_params p) -> setup_request {
                return setup_request{ .standard_isoch_delay_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::standard).set(recipient::device),
                    .bRequest      = req::set_isoch_delay,
                    .delay_ns      = p.delay_ns,
                    .wIndex        = 0,
                    .wLength       = 0,
                }};
            }

            LM_USB_DEFINE_VIEW(as_get_descriptor,           standard_descriptor_fields,           sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::get_descriptor && sr.fields.bmRequestType.dir() == direction::device_to_host)
            LM_USB_DEFINE_VIEW(as_get_string_descriptor,    standard_string_descriptor_fields,    sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::get_descriptor && sr.fields.bmRequestType.dir() == direction::device_to_host && sr.fields.wIndex == (u8)desc::string)
            LM_USB_DEFINE_VIEW(as_get_interface_descriptor, standard_interface_descriptor_fields, sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::get_descriptor && sr.fields.bmRequestType.dir() == direction::device_to_host && sr.fields.wIndex == (u8)desc::interface)
            LM_USB_DEFINE_VIEW(as_set_descriptor,           standard_descriptor_fields,           sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_descriptor && sr.fields.bmRequestType.dir() == direction::host_to_device)
            LM_USB_DEFINE_VIEW(as_set_string_descriptor,    standard_string_descriptor_fields,    sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_descriptor && sr.fields.bmRequestType.dir() == direction::host_to_device && sr.fields.wIndex == (u8)desc::string)
            LM_USB_DEFINE_VIEW(as_set_interface_descriptor, standard_interface_descriptor_fields, sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_descriptor && sr.fields.bmRequestType.dir() == direction::host_to_device && sr.fields.wIndex == (u8)desc::interface)
            LM_USB_DEFINE_VIEW(as_set_feature,              standard_feature_fields,              sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_feature)
            LM_USB_DEFINE_VIEW(as_clear_feature,            standard_feature_fields,              sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::clear_feature)
            LM_USB_DEFINE_VIEW(as_set_address,              standard_address_fields,              sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_address)
            LM_USB_DEFINE_VIEW(as_set_configuration,        standard_configuration_fields,        sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_configuration)
            LM_USB_DEFINE_VIEW(as_get_configuration,        standard_configuration_fields,        sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::get_configuration)
            LM_USB_DEFINE_VIEW(as_get_interface,            standard_interface_fields,            sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::get_interface)
            LM_USB_DEFINE_VIEW(as_set_interface,            standard_interface_fields,            sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_interface)
            LM_USB_DEFINE_VIEW(as_get_status,               standard_status_fields,               sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::get_status)
            LM_USB_DEFINE_VIEW(as_synch_frame,              standard_synch_frame_fields,          sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::synch_frame)
            LM_USB_DEFINE_VIEW(as_set_sel,                  standard_sel_fields,                  sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_sel)
            LM_USB_DEFINE_VIEW(as_set_isoch_delay,          standard_isoch_delay_fields,          sr.fields.bmRequestType.cat() == category::standard && sr.fields.bRequest == (u8)req::set_isoch_delay)
        };

        // ========================================================================
        // CDC
        // ========================================================================
        struct cdc
        {
            using req          = setup_request_ns::cdc::req;
            using comm_feature = setup_request_ns::cdc::comm_feature;

            struct encapsulated_params { u16 interface_num; u16 length; };
            static constexpr auto send_encapsulated_command(encapsulated_params p) -> setup_request {
                return setup_request{ .cdc_encapsulated_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::send_encapsulated_cmd,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = p.length,
                }};
            }
            static constexpr auto get_encapsulated_response(encapsulated_params p) -> setup_request {
                return setup_request{ .cdc_encapsulated_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_encapsulated_rsp,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = p.length,
                }};
            }

            struct comm_feature_params { u16 interface_num; comm_feature feature; };
            static constexpr auto set_comm_feature(comm_feature_params p) -> setup_request {
                return setup_request{ .cdc_comm_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::set_comm_feature,
                    .feature       = p.feature,
                    .interface_num = p.interface_num,
                    .wLength       = 2,
                }};
            }
            static constexpr auto get_comm_feature(comm_feature_params p) -> setup_request {
                return setup_request{ .cdc_comm_feature_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_comm_feature,
                    .feature       = p.feature,
                    .interface_num = p.interface_num,
                    .wLength       = 2,
                }};
            }
            static constexpr auto clear_comm_feature(comm_feature_params p) -> setup_request {
                return setup_request{ .cdc_comm_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::clear_comm_feature,
                    .feature       = p.feature,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            struct line_coding_params { u16 interface_num; };
            static constexpr auto set_line_coding(line_coding_params p) -> setup_request {
                return setup_request{ .cdc_line_coding_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::set_line_coding,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 7,
                }};
            }
            static constexpr auto get_line_coding(line_coding_params p) -> setup_request {
                return setup_request{ .cdc_line_coding_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_line_coding,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 7,
                }};
            }

            struct control_line_state_params {
                u16  interface_num;
                bool dte_present = false; // bit 0: DTR
                bool carrier_on  = false; // bit 1: RTS/carrier
            };
            static constexpr auto set_control_line_state(control_line_state_params p) -> setup_request {
                return setup_request{ .cdc_control_line_state_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::set_control_line_state,
                    .line_state    = static_cast<u16>((p.dte_present ? 0x01u : 0u) | (p.carrier_on ? 0x02u : 0u)),
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            struct send_break_params {
                u16 interface_num;
                u16 duration_ms; // 0xFFFF = indefinite, 0x0000 = stop
            };
            static constexpr auto send_break(send_break_params p) -> setup_request {
                return setup_request{ .cdc_send_break_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::send_break,
                    .duration_ms   = p.duration_ms,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            LM_USB_DEFINE_VIEW(as_send_encapsulated_command,  cdc_encapsulated_fields,         sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::send_encapsulated_cmd  && sr.fields.bmRequestType.dir() == direction::host_to_device)
            LM_USB_DEFINE_VIEW(as_get_encapsulated_response,  cdc_encapsulated_fields,         sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_encapsulated_rsp   && sr.fields.bmRequestType.dir() == direction::device_to_host)
            LM_USB_DEFINE_VIEW(as_set_comm_feature,           cdc_comm_feature_fields,         sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_comm_feature)
            LM_USB_DEFINE_VIEW(as_get_comm_feature,           cdc_comm_feature_fields,         sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_comm_feature)
            LM_USB_DEFINE_VIEW(as_clear_comm_feature,         cdc_comm_feature_fields,         sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::clear_comm_feature)
            LM_USB_DEFINE_VIEW(as_set_line_coding,            cdc_line_coding_fields,          sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_line_coding)
            LM_USB_DEFINE_VIEW(as_get_line_coding,            cdc_line_coding_fields,          sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_line_coding)
            LM_USB_DEFINE_VIEW(as_set_control_line_state,     cdc_control_line_state_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_control_line_state)
            LM_USB_DEFINE_VIEW(as_send_break,                 cdc_send_break_fields,           sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::send_break)
        };

        // ========================================================================
        // HID
        // ========================================================================
        struct hid
        {
            using req         = setup_request_ns::hid::req;
            using report_type = setup_request_ns::hid::report_type;
            using protocol    = setup_request_ns::hid::protocol;

            struct report_params {
                u16         interface_num;
                u8          report_id   = 0;
                report_type type_       = report_type::input;
                u16         length      = 0;
            };
            static constexpr auto get_report(report_params p) -> setup_request {
                return setup_request{ .hid_report_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_report,
                    .report_id     = p.report_id,
                    .report_type_  = p.type_,
                    .interface_num = p.interface_num,
                    .wLength       = p.length,
                }};
            }
            static constexpr auto set_report(report_params p) -> setup_request {
                return setup_request{ .hid_report_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::set_report,
                    .report_id     = p.report_id,
                    .report_type_  = p.type_,
                    .interface_num = p.interface_num,
                    .wLength       = p.length,
                }};
            }

            struct idle_params {
                u16 interface_num;
                u8  report_id = 0;
                u8  duration  = 0; // in 4ms units; 0 = indefinite
            };
            static constexpr auto get_idle(idle_params p) -> setup_request {
                return setup_request{ .hid_idle_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_idle,
                    .report_id     = p.report_id,
                    .duration      = p.duration,
                    .interface_num = p.interface_num,
                    .wLength       = 1,
                }};
            }
            static constexpr auto set_idle(idle_params p) -> setup_request {
                return setup_request{ .hid_idle_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::set_idle,
                    .report_id     = p.report_id,
                    .duration      = p.duration,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            struct protocol_params { u16 interface_num; };
            static constexpr auto get_protocol(protocol_params p) -> setup_request {
                return setup_request{ .hid_protocol_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_protocol,
                    .protocol_     = protocol::report,
                    .interface_num = p.interface_num,
                    .wLength       = 1,
                }};
            }

            struct set_protocol_params { u16 interface_num; protocol proto; };
            static constexpr auto set_protocol(set_protocol_params p) -> setup_request {
                return setup_request{ .hid_protocol_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::set_protocol,
                    .protocol_     = p.proto,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            LM_USB_DEFINE_VIEW(as_get_report,    hid_report_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_report   && sr.fields.bmRequestType.dir() == direction::device_to_host)
            LM_USB_DEFINE_VIEW(as_set_report,    hid_report_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_report   && sr.fields.bmRequestType.dir() == direction::host_to_device)
            LM_USB_DEFINE_VIEW(as_get_idle,      hid_idle_fields,     sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_idle     && sr.fields.bmRequestType.dir() == direction::device_to_host)
            LM_USB_DEFINE_VIEW(as_set_idle,      hid_idle_fields,     sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_idle     && sr.fields.bmRequestType.dir() == direction::host_to_device)
            LM_USB_DEFINE_VIEW(as_get_protocol,  hid_protocol_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_protocol && sr.fields.bmRequestType.dir() == direction::device_to_host)
            LM_USB_DEFINE_VIEW(as_set_protocol,  hid_protocol_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_protocol && sr.fields.bmRequestType.dir() == direction::host_to_device)
        };

        // ========================================================================
        // MSC
        // ========================================================================
        struct msc
        {
            using req = setup_request_ns::msc::req;

            struct interface_params { u16 interface_num; };
            static constexpr auto get_max_lun(interface_params p) -> setup_request {
                return setup_request{ .msc_get_max_lun_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_max_lun,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 1,
                }};
            }
            static constexpr auto reset(interface_params p) -> setup_request {
                return setup_request{ .msc_reset_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::reset,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            LM_USB_DEFINE_VIEW(as_get_max_lun, msc_get_max_lun_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_max_lun)
            LM_USB_DEFINE_VIEW(as_reset,        msc_reset_fields,       sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::reset)
        };

        // ========================================================================
        // DFU
        // ========================================================================
        struct dfu
        {
            using req = setup_request_ns::dfu::req;

            struct detach_params { u16 interface_num; u16 timeout_ms; };
            static constexpr auto detach(detach_params p) -> setup_request {
                return setup_request{ .dfu_detach_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::detach,
                    .timeout_ms    = p.timeout_ms,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            struct transfer_params { u16 interface_num; u16 block_num; u16 length; };
            static constexpr auto download(transfer_params p) -> setup_request {
                return setup_request{ .dfu_transfer_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::download,
                    .block_num     = p.block_num,
                    .interface_num = p.interface_num,
                    .wLength       = p.length,
                }};
            }
            static constexpr auto upload(transfer_params p) -> setup_request {
                return setup_request{ .dfu_transfer_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::upload,
                    .block_num     = p.block_num,
                    .interface_num = p.interface_num,
                    .wLength       = p.length,
                }};
            }

            struct simple_params { u16 interface_num; };
            static constexpr auto get_status(simple_params p) -> setup_request {
                return setup_request{ .dfu_simple_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_status,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 6,
                }};
            }
            static constexpr auto clear_status(simple_params p) -> setup_request {
                return setup_request{ .dfu_simple_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::clear_status,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }
            static constexpr auto get_state(simple_params p) -> setup_request {
                return setup_request{ .dfu_simple_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::get_state,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 1,
                }};
            }
            static constexpr auto abort(simple_params p) -> setup_request {
                return setup_request{ .dfu_simple_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                    .bRequest      = req::abort,
                    .wValue        = 0,
                    .interface_num = p.interface_num,
                    .wLength       = 0,
                }};
            }

            LM_USB_DEFINE_VIEW(as_detach,       dfu_detach_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::detach)
            LM_USB_DEFINE_VIEW(as_download,     dfu_transfer_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::download)
            LM_USB_DEFINE_VIEW(as_upload,       dfu_transfer_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::upload)
            LM_USB_DEFINE_VIEW(as_get_status,   dfu_simple_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_status)
            LM_USB_DEFINE_VIEW(as_clear_status, dfu_simple_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::clear_status)
            LM_USB_DEFINE_VIEW(as_get_state,    dfu_simple_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_state)
            LM_USB_DEFINE_VIEW(as_abort,        dfu_simple_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::abort)
        };

        // ========================================================================
        // AUDIO
        // ========================================================================
        struct audio
        {
            struct v1
            {
                using req = setup_request_ns::audio::v1::req;

                // UAC1: direction is in bRequest itself, not disambiguated by bmRequestType direction.
                struct params { u8 channel_num; u8 control_sel; u8 iface_or_ep; u8 entity_id; u16 length; };

                static constexpr auto set_cur(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                        .bRequest      = req::set_cur,
                        .channel_num   = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep   = p.iface_or_ep, .entity_id   = p.entity_id,
                        .wLength       = p.length,
                    }};
                }
                static constexpr auto get_cur(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest      = req::get_cur,
                        .channel_num   = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep   = p.iface_or_ep, .entity_id   = p.entity_id,
                        .wLength       = p.length,
                    }};
                }
                static constexpr auto set_min(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::set_min, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto get_min(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::get_min, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto set_max(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::set_max, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto get_max(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::get_max, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto set_res(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::set_res, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto get_res(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::get_res, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto set_mem(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::set_mem, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto get_mem(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::get_mem, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }
                static constexpr auto get_stat(params p) -> setup_request {
                    return setup_request{ .audio_v1_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest = req::get_stat, .channel_num = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep = p.iface_or_ep, .entity_id = p.entity_id, .wLength = p.length,
                    }};
                }

                // UAC1 bRequest values uniquely identify both operation and direction.
                LM_USB_DEFINE_VIEW(as_set_cur,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_cur)
                LM_USB_DEFINE_VIEW(as_get_cur,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_cur)
                LM_USB_DEFINE_VIEW(as_set_min,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_min)
                LM_USB_DEFINE_VIEW(as_get_min,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_min)
                LM_USB_DEFINE_VIEW(as_set_max,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_max)
                LM_USB_DEFINE_VIEW(as_get_max,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_max)
                LM_USB_DEFINE_VIEW(as_set_res,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_res)
                LM_USB_DEFINE_VIEW(as_get_res,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_res)
                LM_USB_DEFINE_VIEW(as_set_mem,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_mem)
                LM_USB_DEFINE_VIEW(as_get_mem,  audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_mem)
                LM_USB_DEFINE_VIEW(as_get_stat, audio_v1_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_stat)
            };

            struct v2
            {
                using req = setup_request_ns::audio::v2::req;

                struct params { u8 channel_num; u8 control_sel; u8 iface_or_ep; u8 entity_id; u16 length; };
                static constexpr auto cur(direction dir, params p) -> setup_request {
                    return setup_request{ .audio_v2_fields = {
                        .bmRequestType = type{}.set(dir).set(category::class_specific).set(recipient::interface),
                        .bRequest      = req::cur,
                        .channel_num   = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep   = p.iface_or_ep, .entity_id   = p.entity_id,
                        .wLength       = p.length,
                    }};
                }
                static constexpr auto range(params p) -> setup_request {
                    return setup_request{ .audio_v2_fields = {
                        .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::interface),
                        .bRequest      = req::range,
                        .channel_num   = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep   = p.iface_or_ep, .entity_id   = p.entity_id,
                        .wLength       = p.length,
                    }};
                }
                static constexpr auto mem(direction dir, params p) -> setup_request {
                    return setup_request{ .audio_v2_fields = {
                        .bmRequestType = type{}.set(dir).set(category::class_specific).set(recipient::interface),
                        .bRequest      = req::mem,
                        .channel_num   = p.channel_num, .control_sel = p.control_sel,
                        .iface_or_ep   = p.iface_or_ep, .entity_id   = p.entity_id,
                        .wLength       = p.length,
                    }};
                }

                LM_USB_DEFINE_VIEW(as_cur,   audio_v2_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::cur)
                LM_USB_DEFINE_VIEW(as_range,  audio_v2_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::range)
                LM_USB_DEFINE_VIEW(as_mem,   audio_v2_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::mem)
            };
        };

        // ========================================================================
        // HUB
        // ========================================================================
        struct hub
        {
            using req          = setup_request_ns::hub::req;
            using hub_feature  = setup_request_ns::hub::hub_feature;
            using port_feature = setup_request_ns::hub::port_feature;

            static constexpr auto get_status_hub() -> setup_request {
                return setup_request{ .hub_status_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::device),
                    .bRequest      = req::get_status,
                    .wValue        = 0, .wIndex = 0, .wLength = 4,
                }};
            }

            struct port_status_params { u8 port_num; };
            static constexpr auto get_status_port(port_status_params p) -> setup_request {
                return setup_request{ .hub_status_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::get_status,
                    .wValue        = 0,
                    .wIndex        = p.port_num,
                    .wLength       = 4,
                }};
            }

            struct hub_feature_params { hub_feature feature; };
            static constexpr auto set_feature_hub(hub_feature_params p) -> setup_request {
                return setup_request{ .hub_hub_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::device),
                    .bRequest      = req::set_feature,
                    .feature       = p.feature,
                    .wIndex        = 0,
                    .wLength       = 0,
                }};
            }
            static constexpr auto clear_feature_hub(hub_feature_params p) -> setup_request {
                return setup_request{ .hub_hub_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::device),
                    .bRequest      = req::clear_feature,
                    .feature       = p.feature,
                    .wIndex        = 0,
                    .wLength       = 0,
                }};
            }

            struct port_feature_params { u8 port_num; port_feature feature; u8 selector = 0; };
            static constexpr auto set_feature_port(port_feature_params p) -> setup_request {
                return setup_request{ .hub_port_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::set_feature,
                    .feature       = p.feature,
                    .port_num      = p.port_num,
                    .selector      = p.selector,
                    .wLength       = 0,
                }};
            }
            static constexpr auto clear_feature_port(port_feature_params p) -> setup_request {
                return setup_request{ .hub_port_feature_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::clear_feature,
                    .feature       = p.feature,
                    .port_num      = p.port_num,
                    .selector      = p.selector,
                    .wLength       = 0,
                }};
            }

            struct descriptor_params { u8 descriptor_type = 0x29; u16 length = 0; };
            static constexpr auto get_descriptor(descriptor_params p) -> setup_request {
                return setup_request{ .hub_descriptor_fields = {
                    .bmRequestType   = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::device),
                    .bRequest        = req::get_descriptor,
                    .index           = 0,
                    .descriptor_type = p.descriptor_type,
                    .wIndex          = 0,
                    .wLength         = p.length,
                }};
            }

            struct tt_params { u8 tt_port; u16 dev_ep = 0; };
            static constexpr auto clear_tt_buffer(tt_params p) -> setup_request {
                return setup_request{ .hub_tt_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::clear_tt_buffer,
                    .dev_ep        = p.dev_ep,
                    .tt_port       = p.tt_port,
                    .wLength       = 0,
                }};
            }
            static constexpr auto reset_tt(tt_params p) -> setup_request {
                return setup_request{ .hub_tt_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::reset_tt,
                    .dev_ep        = 0, .tt_port = p.tt_port, .wLength = 0,
                }};
            }
            static constexpr auto get_tt_state(tt_params p) -> setup_request {
                return setup_request{ .hub_tt_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::get_tt_state,
                    .dev_ep        = 0, .tt_port = p.tt_port, .wLength = 0,
                }};
            }
            static constexpr auto stop_tt(tt_params p) -> setup_request {
                return setup_request{ .hub_tt_fields = {
                    .bmRequestType = type{}.set(direction::host_to_device).set(category::class_specific).set(recipient::other),
                    .bRequest      = req::stop_tt,
                    .dev_ep        = 0, .tt_port = p.tt_port, .wLength = 0,
                }};
            }

            LM_USB_DEFINE_VIEW(as_get_status_hub,     hub_status_fields,       sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_status    && sr.fields.bmRequestType.rec() == recipient::device)
            LM_USB_DEFINE_VIEW(as_get_status_port,    hub_status_fields,       sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_status    && sr.fields.bmRequestType.rec() == recipient::other)
            LM_USB_DEFINE_VIEW(as_set_feature_hub,    hub_hub_feature_fields,  sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_feature   && sr.fields.bmRequestType.rec() == recipient::device)
            LM_USB_DEFINE_VIEW(as_clear_feature_hub,  hub_hub_feature_fields,  sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::clear_feature && sr.fields.bmRequestType.rec() == recipient::device)
            LM_USB_DEFINE_VIEW(as_set_feature_port,   hub_port_feature_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::set_feature   && sr.fields.bmRequestType.rec() == recipient::other)
            LM_USB_DEFINE_VIEW(as_clear_feature_port, hub_port_feature_fields, sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::clear_feature && sr.fields.bmRequestType.rec() == recipient::other)
            LM_USB_DEFINE_VIEW(as_get_descriptor,     hub_descriptor_fields,   sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_descriptor)
            LM_USB_DEFINE_VIEW(as_clear_tt_buffer,    hub_tt_fields,           sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::clear_tt_buffer)
            LM_USB_DEFINE_VIEW(as_reset_tt,           hub_tt_fields,           sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::reset_tt)
            LM_USB_DEFINE_VIEW(as_get_tt_state,       hub_tt_fields,           sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::get_tt_state)
            LM_USB_DEFINE_VIEW(as_stop_tt,            hub_tt_fields,           sr.fields.bmRequestType.cat() == category::class_specific && sr.fields.bRequest == (u8)req::stop_tt)
        };

        // ========================================================================
        // VENDOR
        // ========================================================================
        struct vendor
        {
            using req = setup_request_ns::vendor::req;

            struct webusb_params { u16 url_index; u16 length = 0; };
            static constexpr auto get_url(webusb_params p) -> setup_request {
                return setup_request{ .vendor_webusb_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::vendor).set(recipient::device),
                    .bRequest      = req::webusb_get_url,
                    .url_index     = p.url_index,
                    .wIndex        = 0x0002,
                    .wLength       = p.length,
                }};
            }

            struct ms_os_20_params { u16 length = 0; };
            static constexpr auto get_ms_os_20_descriptor(ms_os_20_params p) -> setup_request {
                return setup_request{ .vendor_ms_os_20_fields = {
                    .bmRequestType = type{}.set(direction::device_to_host).set(category::vendor).set(recipient::device),
                    .bRequest      = req::ms_os_20_descriptor,
                    .wValue        = 0,
                    .wIndex        = 0x0007,
                    .wLength       = p.length,
                }};
            }

            LM_USB_DEFINE_VIEW(as_get_url,               vendor_webusb_fields,   sr.fields.bmRequestType.cat() == category::vendor && sr.fields.bRequest == (u8)req::webusb_get_url)
            LM_USB_DEFINE_VIEW(as_get_ms_os_20_descriptor, vendor_ms_os_20_fields, sr.fields.bmRequestType.cat() == category::vendor && sr.fields.bRequest == (u8)req::ms_os_20_descriptor)
        };

        #undef LM_USB_DEFINE_VIEW
    };
    static_assert(sizeof(setup_request) == 8, "Setup request struct must be exactly 8 bytes");
    using sr = setup_request;

    struct device_descriptor
    {
        enum class usb_spec : u16 {
            v1_1 = 0x0110,
            v2_0 = 0x0200,
            v3_0 = 0x0300,
            v3_1 = 0x0310,
            v3_2 = 0x0320,
        };

        enum class device_class : u8 {
            deferred_to_interface = 0x00,
            audio                 = 0x01,
            cdc                   = 0x02,
            hid                   = 0x03,
            physical              = 0x05,
            image                 = 0x06,
            printer               = 0x07,
            mass_storage          = 0x08,
            hub                   = 0x09,
            cdc_data              = 0x0A,
            smart_card            = 0x0B,
            content_security      = 0x0C,
            video                 = 0x0E,
            personal_healthcare   = 0x0F,
            audio_video           = 0x10,
            diagnostic            = 0xDC,
            wireless              = 0xE0,
            miscellaneous         = 0xEF,
            application_specific  = 0xFE,
            vendor_specific       = 0xFF
        };

        struct audio {
            enum class subclass : u8 {
                undefined       = 0x00,
                audio_control   = 0x01,
                audio_streaming = 0x02,
                midi_streaming  = 0x03
            };
            enum class protocol : u8 {
                undefined = 0x00,
                v2_0      = 0x20,
                v3_0      = 0x30
            };
        };

        struct cdc {
            enum class subclass : u8 {
                direct_line      = 0x01,
                acm              = 0x02,
                telephone        = 0x03,
                multi_channel    = 0x04,
                capi             = 0x05,
                ecm              = 0x06,
                atm              = 0x07,
                wireless_handset = 0x08,
                device_mgmt      = 0x09,
                mobile_dl        = 0x0A,
                obex             = 0x0B,
                eem              = 0x0C,
                ncm              = 0x0D
            };
            enum class protocol : u8 {
                no_protocol        = 0x00,
                at_commands        = 0x01,
                at_pcca101         = 0x02,
                at_pcca101_annex_o = 0x03,
                at_gsm_0707        = 0x04,
                at_3gpp_27007      = 0x05,
                at_cdma            = 0x06,
                eem                = 0x07,
                external           = 0xFE,
                vendor             = 0xFF
            };
        };

        struct hid {
            enum class subclass : u8 { no_subclass = 0x00, boot_interface = 0x01 };
            enum class protocol : u8 { none = 0x00, keyboard = 0x01, mouse = 0x02 };
        };

        struct mass_storage {
            enum class subclass : u8 {
                rbc       = 0x01,
                sff_8020i = 0x02,
                qic_157   = 0x03,
                ufi       = 0x04,
                sff_8070i = 0x05,
                scsi      = 0x06,
                lsd_fs    = 0x07,
                ieee_1667 = 0x08
            };
            enum class protocol : u8 {
                cbi_with_completion = 0x00,
                cbi_no_completion   = 0x01,
                bulk_only           = 0x50,
                uasp                = 0x62
            };
        };

        struct hub {
            enum class subclass : u8 { unused = 0x00 };
            enum class protocol : u8 {
                full_speed         = 0x00,
                hi_speed_single_tt = 0x01,
                hi_speed_multi_tt  = 0x02
            };
        };

        // Wireless protocol values overlap between subclasses, so they are nested
        // under their respective subclass contexts rather than a single flat enum.
        struct wireless {
            enum class subclass : u8 {
                rf_controller = 0x01,
                wire_adapter  = 0x02
            };
            struct rf {
                enum class protocol : u8 {
                    bluetooth     = 0x01,
                    uwb           = 0x02,
                    remote_ndis   = 0x03,
                    bluetooth_amp = 0x04
                };
            };
            struct wire_adapter {
                enum class protocol : u8 {
                    host_ctrl_data   = 0x01,
                    device_ctrl_data = 0x02,
                    device_iso       = 0x03
                };
            };
        };

        struct misc {
            enum class subclass : u8 {
                sync             = 0x01,
                common           = 0x02,
                cable_association = 0x03
            };
            enum class protocol : u8 {
                none             = 0x00,
                iad              = 0x01,
                deferred_routing = 0x02
            };
        };

        union {
            struct {
                u8           length                        = 18;
                u8           descriptor_type               = 0x01;
                usb_spec     usb_specification             = usb_spec::v2_0;
                device_class base_class                    = device_class::deferred_to_interface;
                u8           subclass                      = 0;
                u8           protocol                      = 0;
                u8           control_endpoint_max_packet_size = 64;
                u16          vendor_id                     = 0;
                u16          product_id                    = 0;
                u16          device_release_version        = 0;
                u8           manufacturer_string_index     = 0;
                u8           product_string_index          = 0;
                u8           serial_number_string_index    = 0;
                u8           configuration_count           = 1;
            } fields;

            struct {
                u8  bLength;
                u8  bDescriptorType;
                u16 bcdUSB;
                u8  bDeviceClass;
                u8  bDeviceSubClass;
                u8  bDeviceProtocol;
                u8  bMaxPacketSize0;
                u16 idVendor;
                u16 idProduct;
                u16 bcdDevice;
                u8  iManufacturer;
                u8  iProduct;
                u8  iSerialNumber;
                u8  bNumConfigurations;
            } spec;

            std::array<u8, 18> bytes;
        };

        static constexpr auto zero() -> device_descriptor {
            auto ret = device_descriptor{};
            ret.bytes = {};
            return ret;
        }

        static constexpr auto bcd_version(u8 major, u8 minor) -> u16 {
            return static_cast<u16>((major << 8) | ((minor / 10) << 4) | (minor % 10));
        }

        // ========================================================================
        // STRONGLY TYPED CONTEXTUAL CLASS MUTATORS
        // ========================================================================
        constexpr auto as_audio(audio::subclass sub, audio::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::audio;
            fields.subclass   = static_cast<u8>(sub);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_cdc(cdc::subclass sub, cdc::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::cdc;
            fields.subclass   = static_cast<u8>(sub);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_hid(hid::subclass sub, hid::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::hid;
            fields.subclass   = static_cast<u8>(sub);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_mass_storage(mass_storage::subclass sub, mass_storage::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::mass_storage;
            fields.subclass   = static_cast<u8>(sub);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_hub(hub::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::hub;
            fields.subclass   = static_cast<u8>(hub::subclass::unused);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_wireless_rf(wireless::rf::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::wireless;
            fields.subclass   = static_cast<u8>(wireless::subclass::rf_controller);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_wireless_wire_adapter(wireless::wire_adapter::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::wireless;
            fields.subclass   = static_cast<u8>(wireless::subclass::wire_adapter);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }

        constexpr auto as_miscellaneous(misc::subclass sub, misc::protocol proto) -> device_descriptor& {
            fields.base_class = device_class::miscellaneous;
            fields.subclass   = static_cast<u8>(sub);
            fields.protocol   = static_cast<u8>(proto);
            return *this;
        }
    }; static_assert(sizeof(device_descriptor) == 18, "Structure layout footprint mismatch!");
    using dd = device_descriptor;

    #pragma pack(pop)
}
