
// Thingies to make low-level usb bitbangin' not insufferable.
#pragma once

#include <span>
#include <expected>
#include <string_view>
#include <cstring>
#include <array>

// General definitions.
namespace lm::usb
{
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

    // TODO: Do we even need this?? I think I'd rather have it wrapped in a type with a direction bit.
    // When ep direction is IN we need to do ep_dir_in | ep_idx in the configuration descriptor.
    constexpr auto ep_dir_in = 0x80_u8;
    
    // Transfer type — endpoint descriptor bmAttributes bits[1:0].
    // Values match USB 2.0 spec table 9-13 / tusb_xfer_type_t.
    enum class xfer_type_t : u8 {
        control     = 0,
        isochronous = 1,
        bulk        = 2,
        interrupt   = 3,
    };

    // This is usually hardcoded per-transport per-architecture.
    // The actual endpoint assignment
    struct ep
    {
        bool supports_isochronous : 1 = true;
        bool supports_bulk : 1        = true;
        bool supports_interrupt : 1   = true;

        u16 mps = 0; // Max packet size.

        static constexpr auto unsupported() -> ep
        { return { .supports_isochronous = false, .supports_bulk = false, .supports_interrupt = false }; }
    };

    // Endpoint 0 omitted, its implicit. If an endpoint is not supported just mark it as ep::unsupported().
    struct eps
    {
        // TODO: check bounds
        constexpr auto in(u8 idx) const -> ep const& { return _in[idx - 1]; }
        constexpr auto in(u8 idx)       -> ep&       { return _in[idx - 1]; }
        constexpr auto out(u8 idx) const -> ep const& { return _out[idx - 1]; }
        constexpr auto out(u8 idx)       -> ep&       { return _out[idx - 1]; }

        std::array<ep, 15> _in;
        std::array<ep, 15> _out;
    };

    // Isochronous synchronisation type — bmAttributes bits[3:2].
    // Meaningful only when xfer_type_t == isochronous.
    enum class iso_sync_t : u8 {
        no_sync      = 0,
        asynchronous = 1,
        adaptive     = 2,
        synchronous  = 3,
    };

    // Isochronous endpoint usage type — bmAttributes bits[5:4].
    // Meaningful only when xfer_type_t == isochronous.
    enum class iso_usage_t : u8 {
        data        = 0,
        feedback    = 1,
        implicit_fb = 2,
    };

    // Bus speed — tusb_speed_t.
    enum class speed_t : u8 {
        full    = 0x00,
        low     = 0x01,
        high    = 0x02,
        auto_   = 0xAA,
        invalid = 0xFF,
    };

    // Stack role — tusb_role_t.
    enum class role_t : u8 {
        invalid = 0,
        device  = 1,
        host    = 2,
    };
}

// Setup request stuff.
namespace lm::usb
{
    #pragma pack(push, 1)

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

    #pragma pack(pop)
}

// Device descriptor stuff.
namespace lm::usb
{
    #pragma pack(push, 1)

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

// ============================================================================
// lm::usb::cd — configuration descriptor struct layer
// ============================================================================
namespace lm::usb::cd
{
    #pragma pack(push, 1)

    // Descriptor type byte (bDescriptorType) — full USB standard + class-specific set.
    // Distinct from setup_request_ns::standard::desc which is used in the setup-request
    // wValue context. This enum is for the bDescriptorType field inside descriptor structs.
    enum class desc_type_t : u8 {
        device                      = 0x01,
        configuration               = 0x02,
        string                      = 0x03,
        interface_                  = 0x04, // trailing underscore: avoids keyword conflict
        endpoint                    = 0x05,
        device_qualifier            = 0x06,
        other_speed_config          = 0x07,
        interface_power             = 0x08,
        otg                         = 0x09,
        debug                       = 0x0A,
        interface_association       = 0x0B,
        bos                         = 0x0F,
        device_capability           = 0x10,
        functional                  = 0x21, // DFU functional / HID class descriptor
        cs_device                   = 0x21,
        cs_configuration            = 0x22,
        cs_string                   = 0x23,
        cs_interface                = 0x24,
        cs_endpoint                 = 0x25,
        superspeed_ep_companion     = 0x30,
        superspeed_iso_ep_companion = 0x31,
    };

    // ========================================================================
    // Standard Configuration Descriptor — 9 bytes, bDescriptorType = 0x02
    // wTotalLength and bNumInterfaces are patched by descriptor_builder::build().
    // ========================================================================

    // Configuration bmAttributes byte.
    // Bit 7 is always 1 (legacy reserved). Bit 6 = self-powered. Bit 5 = remote wakeup.
    struct config_attr_t {
        u8 raw = 0x80;
        static constexpr auto bus_powered()                -> config_attr_t { return {0x80}; }
        static constexpr auto self_powered()               -> config_attr_t { return {0xC0}; }
        static constexpr auto remote_wakeup()              -> config_attr_t { return {0xA0}; }
        static constexpr auto self_powered_remote_wakeup() -> config_attr_t { return {0xE0}; }
    };

    struct configuration_descriptor
    {
        union {
            struct {
                u8            length               = 9;
                desc_type_t   descriptor_type      = desc_type_t::configuration;
                u16           total_length         = 9;  // patched by build()
                u8            num_interfaces       = 0;  // patched by build()
                u8            configuration_value  = 1;
                u8            configuration_string = 0;  // string descriptor index; 0 = none
                config_attr_t attributes           = config_attr_t::bus_powered();
                u8            max_power_x2         = 50; // in 2 mA units (50 = 100 mA)
            } fields;

            struct {
                u8  bLength;
                u8  bDescriptorType;
                u16 wTotalLength;
                u8  bNumInterfaces;
                u8  bConfigurationValue;
                u8  iConfiguration;
                u8  bmAttributes;
                u8  bMaxPower;
            } spec;

            std::array<u8, 9> bytes;
        };

        constexpr auto set_max_power_ma(u16 milliamps) -> configuration_descriptor& {
            fields.max_power_x2 = static_cast<u8>(milliamps / 2u);
            return *this;
        }
    };
    static_assert(sizeof(configuration_descriptor) == 9, "configuration_descriptor layout mismatch");

    // ========================================================================
    // Standard Interface Descriptor — 9 bytes, bDescriptorType = 0x04
    // ========================================================================

    struct interface_descriptor
    {
        union {
            struct {
                u8            length              = 9;
                desc_type_t   descriptor_type     = desc_type_t::interface_;
                u8            interface_number    = 0;
                u8            alternate_setting   = 0;
                u8            num_endpoints       = 0;
                lm::usb::device_descriptor::device_class interface_class
                    = lm::usb::device_descriptor::device_class::deferred_to_interface;
                u8            interface_subclass  = 0;
                u8            interface_protocol  = 0;
                u8            interface_string    = 0;  // string descriptor index; 0 = none
            } fields;

            struct {
                u8  bLength;
                u8  bDescriptorType;
                u8  bInterfaceNumber;
                u8  bAlternateSetting;
                u8  bNumEndpoints;
                u8  bInterfaceClass;
                u8  bInterfaceSubClass;
                u8  bInterfaceProtocol;
                u8  iInterface;
            } spec;

            std::array<u8, 9> bytes;
        };
    };
    static_assert(sizeof(interface_descriptor) == 9, "interface_descriptor layout mismatch");

    // ========================================================================
    // Standard Endpoint Descriptor — 7 bytes, bDescriptorType = 0x05
    // ========================================================================

    // bmAttributes accessor/builder.
    // bits[1:0] = transfer type, bits[3:2] = ISO sync, bits[5:4] = ISO usage.
    struct ep_attr_t {
        u8 raw = 0;
        constexpr ep_attr_t() = default;
        constexpr ep_attr_t(xfer_type_t xfer,
                            iso_sync_t  sync  = iso_sync_t::no_sync,
                            iso_usage_t usage = iso_usage_t::data) noexcept
            : raw(static_cast<u8>(  (static_cast<u8>(xfer)  & 0x03)
                                  | ((static_cast<u8>(sync)  & 0x03) << 2)
                                  | ((static_cast<u8>(usage) & 0x03) << 4)))
        {}
        constexpr auto xfer()  const -> xfer_type_t { return static_cast<xfer_type_t>( raw       & 0x03); }
        constexpr auto sync()  const -> iso_sync_t  { return static_cast<iso_sync_t> ((raw >> 2) & 0x03); }
        constexpr auto usage() const -> iso_usage_t { return static_cast<iso_usage_t>((raw >> 4) & 0x03); }
    };

    struct endpoint_descriptor
    {
        union {
            struct {
                u8          length           = 7;
                desc_type_t descriptor_type  = desc_type_t::endpoint;
                u8          endpoint_address = 0;  // EP_DIR_IN | ep_num for IN; ep_num for OUT
                ep_attr_t   attributes       = ep_attr_t{ xfer_type_t::bulk };
                u16         max_packet_size  = 64;
                u8          interval         = 0;  // 0 for bulk; frames/microframes for interrupt/ISO
            } fields;

            struct {
                u8  bLength;
                u8  bDescriptorType;
                u8  bEndpointAddress;
                u8  bmAttributes;
                u16 wMaxPacketSize;
                u8  bInterval;
            } spec;

            std::array<u8, 7> bytes;
        };
    };
    static_assert(sizeof(endpoint_descriptor) == 7, "endpoint_descriptor layout mismatch");

    // ========================================================================
    // Interface Association Descriptor (IAD ECN) — 8 bytes, bDescriptorType = 0x0B
    // ========================================================================

    struct iad_descriptor
    {
        union {
            struct {
                u8          length             = 8;
                desc_type_t descriptor_type    = desc_type_t::interface_association;
                u8          first_interface    = 0;
                u8          interface_count    = 2;
                lm::usb::device_descriptor::device_class function_class
                    = lm::usb::device_descriptor::device_class::cdc;
                u8          function_subclass  = 0;
                u8          function_protocol  = 0;
                u8          function_string    = 0;
            } fields;

            struct {
                u8  bLength;
                u8  bDescriptorType;
                u8  bFirstInterface;
                u8  bInterfaceCount;
                u8  bFunctionClass;
                u8  bFunctionSubClass;
                u8  bFunctionProtocol;
                u8  iFunction;
            } spec;

            std::array<u8, 8> bytes;
        };
    };
    static_assert(sizeof(iad_descriptor) == 8, "iad_descriptor layout mismatch");

    // ========================================================================
    // CDC Functional Descriptors — USB CDC 1.2 §5.2.3
    // ========================================================================

    namespace cdc
    {
        // bDescriptorSubtype values for class-specific CDC interface descriptors.
        enum class func_subtype : u8 {
            header                        = 0x00,
            call_management               = 0x01,
            acm                           = 0x02,
            direct_line                   = 0x03,
            telephone_ringer              = 0x04,
            telephone_call_and_line_state = 0x05,
            union_                        = 0x06, // trailing underscore: avoids keyword conflict
            country_selection             = 0x07,
            telephone_opmode              = 0x08,
            usb_terminal                  = 0x09,
            network_channel               = 0x0A,
            protocol_unit                 = 0x0B,
            extension_unit                = 0x0C,
            multi_channel                 = 0x0D,
            capi_control                  = 0x0E,
            ethernet_networking           = 0x0F,
            atm_networking                = 0x10,
            wireless_handset              = 0x11,
            mobile_dl                     = 0x12,
            mdlm_detail                   = 0x13,
            device_mgmt                   = 0x14,
            obex                          = 0x15,
            cmd_set                       = 0x16,
            cmd_set_detail                = 0x17,
            telephone_control             = 0x18,
            obex_service_id               = 0x19,
            ncm                           = 0x1A,
        };

        // CDC Header Functional Descriptor — 5 bytes.
        // Declares the CDC spec version implemented (bcdCDC = 0x0120, CDC 1.20).
        struct header_functional
        {
            union {
                struct {
                    u8           length          = 5;
                    desc_type_t  descriptor_type = desc_type_t::cs_interface;
                    func_subtype subtype         = func_subtype::header;
                    u16          cdc_version     = 0x0120; // bcdCDC: CDC spec 1.20
                } fields;

                struct {
                    u8  bFunctionLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u16 bcdCDC;
                } spec;

                std::array<u8, 5> bytes;
            };
        };
        static_assert(sizeof(header_functional) == 5, "cdc::header_functional layout mismatch");

        // CDC Call Management Functional Descriptor — 5 bytes.
        // capabilities = 0x00: device does not handle calls itself.
        // data_interface: interface number of the associated CDC Data interface.
        struct call_management_functional
        {
            union {
                struct {
                    u8           length          = 5;
                    desc_type_t  descriptor_type = desc_type_t::cs_interface;
                    func_subtype subtype         = func_subtype::call_management;
                    u8           capabilities    = 0x00; // bit0: manages calls; bit1: can use data interface
                    u8           data_interface  = 0;
                } fields;

                struct {
                    u8  bFunctionLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u8  bmCapabilities;
                    u8  bDataInterface;
                } spec;

                std::array<u8, 5> bytes;
            };
        };
        static_assert(sizeof(call_management_functional) == 5, "cdc::call_management_functional layout mismatch");

        // CDC ACM Functional Descriptor — 4 bytes.
        // capabilities = 0x02: device supports Set/Get_Line_Coding + Set_Control_Line_State.
        struct acm_functional
        {
            union {
                struct {
                    u8           length          = 4;
                    desc_type_t  descriptor_type = desc_type_t::cs_interface;
                    func_subtype subtype         = func_subtype::acm;
                    u8           capabilities    = 0x02;
                } fields;

                struct {
                    u8  bFunctionLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u8  bmCapabilities;
                } spec;

                std::array<u8, 4> bytes;
            };
        };
        static_assert(sizeof(acm_functional) == 4, "cdc::acm_functional layout mismatch");

        // CDC Union Functional Descriptor — 5 bytes (2-interface form).
        // control_interface: the CDC Comm interface number.
        // data_interface: the single subordinate CDC Data interface number.
        struct union_functional
        {
            union {
                struct {
                    u8           length             = 5;
                    desc_type_t  descriptor_type    = desc_type_t::cs_interface;
                    func_subtype subtype            = func_subtype::union_;
                    u8           control_interface  = 0;
                    u8           data_interface     = 0;
                } fields;

                struct {
                    u8  bFunctionLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u8  bControlInterface;
                    u8  bSubordinateInterface0;
                } spec;

                std::array<u8, 5> bytes;
            };
        };
        static_assert(sizeof(union_functional) == 5, "cdc::union_functional layout mismatch");
    } // namespace cdc

    // ========================================================================
    // HID Class Descriptor — USB HID 1.11 §6.2.1
    // ========================================================================

    namespace hid
    {
        // HID-specific bDescriptorType values.
        enum class desc_type : u8 {
            hid      = 0x21,
            report   = 0x22,
            physical = 0x23,
        };

        // HID Descriptor — 6 + 3*N bytes, bDescriptorType = 0x21.
        // N = number of subordinate class descriptors (almost always 1 = report descriptor).
        //
        // Variable-length: no union/bytes pattern.
        // Serialise via builder.append(hid_desc) — the outer struct is a plain wrapper,
        // sizeof(hid_descriptor<N>) == sizeof(fields) under pack(1).
        template<u8 N = 1>
        struct hid_descriptor
        {
            // Each subordinate descriptor entry: 1-byte type + 2-byte length.
            struct entry_t {
                desc_type type;
                u16       descriptor_length;
            };
            static_assert(sizeof(entry_t) == 3, "hid entry_t must be 3 bytes under pack(1)");

            struct {
                u8        length          = static_cast<u8>(6 + 3 * N);
                desc_type descriptor_type = desc_type::hid;
                u16       hid_version     = 0x0111; // bcdHID: HID spec 1.11
                u8        country_code    = 0x00;   // 0 = not localised
                u8        num_descriptors = N;
                entry_t   descriptors[N]  = {};
            } fields;

            // Convenience factory for the standard single-report case.
            static constexpr auto make_report(u16 report_len) -> hid_descriptor<1>
            {
                hid_descriptor<1> d{};
                d.fields.descriptors[0] = { .type = desc_type::report, .descriptor_length = report_len };
                return d;
            }
        };
        static_assert(sizeof(hid_descriptor<1>) == 9,  "hid_descriptor<1> layout mismatch");
        static_assert(sizeof(hid_descriptor<2>) == 12, "hid_descriptor<2> layout mismatch");
    } // namespace hid

    // ========================================================================
    // MIDI Class Descriptors — USB MIDI 1.0 §6.1–§6.2
    // ========================================================================

    namespace midi
    {
        // Class-specific Audio Control interface subtypes.
        enum class cs_ac_subtype : u8 {
            header          = 0x01,
            input_terminal  = 0x02,
            output_terminal = 0x03,
            mixer_unit      = 0x04,
            selector_unit   = 0x05,
            feature_unit    = 0x06,
            processing_unit = 0x07,
            extension_unit  = 0x08,
        };

        // Class-specific MIDI Streaming interface subtypes.
        enum class cs_ms_subtype : u8 {
            header   = 0x01,
            in_jack  = 0x02,
            out_jack = 0x03,
            element  = 0x04,
        };

        // MIDI jack type.
        enum class jack_type : u8 {
            embedded = 0x01,
            external = 0x02,
        };

        // Class-specific MS endpoint subtype.
        enum class cs_ep_subtype : u8 {
            general = 0x01,
        };

        // ------------------------------------------------------------------
        // CS AC Interface Header — 9 bytes.
        // MIDI is a USB Audio subclass; a dummy AC interface header is required.
        // ------------------------------------------------------------------
        struct cs_ac_header
        {
            union {
                struct {
                    u8            length          = 9;
                    desc_type_t   descriptor_type = desc_type_t::cs_interface;
                    cs_ac_subtype subtype         = cs_ac_subtype::header;
                    u16           adc_version     = 0x0100; // bcdADC: USB Audio Class 1.00
                    u16           total_length    = 9;      // wTotalLength of this AC descriptor set
                    u8            collection_size = 1;      // bInCollection: one MS interface follows
                    u8            ms_interface    = 0;      // baInterfaceNr[0]: MS interface number
                } fields;

                struct {
                    u8  bLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u16 bcdADC;
                    u16 wTotalLength;
                    u8  bInCollection;
                    u8  baInterfaceNr0;
                } spec;

                std::array<u8, 9> bytes;
            };
        };
        static_assert(sizeof(cs_ac_header) == 9, "midi::cs_ac_header layout mismatch");

        // ------------------------------------------------------------------
        // CS MS Interface Header — 7 bytes.
        // total_length covers the header + all jack descriptors for this MS interface.
        // Patched by add_midi after calculating the jack block size.
        // ------------------------------------------------------------------
        struct cs_ms_header
        {
            union {
                struct {
                    u8            length          = 7;
                    desc_type_t   descriptor_type = desc_type_t::cs_interface;
                    cs_ms_subtype subtype         = cs_ms_subtype::header;
                    u16           msc_version     = 0x0100; // bcdMSC: MIDI Streaming spec 1.00
                    u16           total_length    = 7;      // patched by add_midi
                } fields;

                struct {
                    u8  bLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u16 bcdMSC;
                    u16 wTotalLength;
                } spec;

                std::array<u8, 7> bytes;
            };
        };
        static_assert(sizeof(cs_ms_header) == 7, "midi::cs_ms_header layout mismatch");

        // ------------------------------------------------------------------
        // MIDI IN Jack Descriptor — 6 bytes (fixed).
        // ------------------------------------------------------------------
        struct in_jack
        {
            union {
                struct {
                    u8            length          = 6;
                    desc_type_t   descriptor_type = desc_type_t::cs_interface;
                    cs_ms_subtype subtype         = cs_ms_subtype::in_jack;
                    jack_type     type_           = jack_type::embedded;
                    u8            jack_id         = 0;
                    u8            jack_string     = 0;
                } fields;

                struct {
                    u8  bLength;
                    u8  bDescriptorType;
                    u8  bDescriptorSubtype;
                    u8  bJackType;
                    u8  bJackID;
                    u8  iJack;
                } spec;

                std::array<u8, 6> bytes;
            };
        };
        static_assert(sizeof(in_jack) == 6, "midi::in_jack layout mismatch");

        // ------------------------------------------------------------------
        // MIDI OUT Jack Descriptor — 7 + 2*N bytes (variable, N = source pin count).
        //
        // Standard USB MIDI class topology always uses N = 1 per OUT jack.
        // Source pins are interleaved per spec: baSourceID[i], BaSourcePin[i].
        //
        // Variable-length: no union/bytes pattern. Serialise via builder.append().
        // ------------------------------------------------------------------
        template<u8 N = 1>
        struct out_jack
        {
            // Per-pin pair: source jack ID + source pin number (1-based).
            struct pin_entry_t { u8 source_id; u8 source_pin; };
            static_assert(sizeof(pin_entry_t) == 2, "pin_entry_t must be 2 bytes under pack(1)");

            struct {
                u8            length          = static_cast<u8>(7 + 2 * N);
                desc_type_t   descriptor_type = desc_type_t::cs_interface;
                cs_ms_subtype subtype         = cs_ms_subtype::out_jack;
                jack_type     type_           = jack_type::embedded;
                u8            jack_id         = 0;
                u8            num_input_pins  = N;
                pin_entry_t   pins[N]         = {}; // interleaved: [baSourceID, BaSourcePin] × N
                u8            jack_string     = 0;
            } fields;
        };
        static_assert(sizeof(out_jack<1>) == 9,  "midi::out_jack<1> layout mismatch");
        static_assert(sizeof(out_jack<2>) == 11, "midi::out_jack<2> layout mismatch");

        // ------------------------------------------------------------------
        // Class-Specific MS Endpoint Descriptor — 4 + N bytes.
        // Lists the embedded jack IDs bound to a bulk endpoint.
        // N = number of cables/jacks on the endpoint; must match the cable count.
        //
        // Variable-length: no union/bytes pattern. Serialise via builder.append().
        //
        // When cable count is not a compile-time constant (e.g. inside add_midi),
        // emit via descriptor_builder::append_bytes() with a stack buffer instead.
        // ------------------------------------------------------------------
        template<u8 N = 1>
        struct cs_endpoint
        {
            struct {
                u8            length          = static_cast<u8>(4 + N);
                desc_type_t   descriptor_type = desc_type_t::cs_endpoint;
                cs_ep_subtype subtype         = cs_ep_subtype::general;
                u8            num_jacks       = N;
                u8            jack_ids[N]     = {};
            } fields;
        };
        static_assert(sizeof(cs_endpoint<1>) == 5, "midi::cs_endpoint<1> layout mismatch");
        static_assert(sizeof(cs_endpoint<4>) == 8, "midi::cs_endpoint<4> layout mismatch");
    } // namespace midi

    // Audio — deferred until next design session.
    namespace audio {}

    #pragma pack(pop)

} // namespace lm::usb::cd

// ============================================================================
// lm::usb — string pool, descriptor_builder, add_* free functions
// ============================================================================
namespace lm::usb
{
    #pragma pack(push, 1)

    // Maximum raw byte count per string entry.
    // 126 ASCII bytes → 252 UTF-16LE bytes, fitting the 254-byte USB string descriptor
    // payload with 2 bytes to spare for the descriptor header.
    inline constexpr u8 MAX_STRING_ENTRY_BYTES = 126;

    // A single interned string entry in the pool.
    // id = 0 indicates an unused slot.
    // Encoding is the consumer's concern — raw bytes stored as-is.
    struct string_entry {
        u8   id     = 0;
        u8   length = 0;
        char data[MAX_STRING_ENTRY_BYTES] = {};
    };

    #pragma pack(pop)

    enum class string_overflow_policy : u8 {
        error,    // return string_error::overflow when content > MAX_STRING_ENTRY_BYTES
        truncate, // copy first MAX_STRING_ENTRY_BYTES bytes and succeed
    };

    enum class string_error : u8 {
        overflow,
        pool_exhausted,
    };

    enum class ep_alloc_error : u8 {
        no_free_ep,
        constraint_violation,
    };

    enum class build_error : u8 {
        buffer_overflow, // config_buf was too small to hold all appended descriptors
    };

    // Endpoint capability matrix — opaque forward declaration.
    // Pass nullptr (no hardware restrictions) for native / unrestricted targets.
    // Format to be defined during the ep_matrix design session (ADR pending).
    struct ep_matrix;

    // ========================================================================
    // descriptor_builder
    //
    // BYOM — all backing storage is caller-provided.
    // The builder writes sequentially into config_buf and interns strings into
    // string_pool. It holds no internal storage of its own.
    // ========================================================================

    struct descriptor_builder
    {
        descriptor_builder(
            cd::configuration_descriptor  config_header,
            std::span<u8>                 config_buf,
            std::span<string_entry>       string_pool,
            device_descriptor&            device_desc,
            const ep_matrix*              ep_constraints
        ) noexcept
            : _config_buf    (config_buf)
            , _string_pool   (string_pool)
            , _device_desc   (device_desc)
            , _ep_constraints(ep_constraints)
        {
            // Append the config header immediately; wTotalLength and bNumInterfaces
            // are at known offsets [2..3] and [4] and will be patched by build().
            append(config_header);
        }

        // Append any trivially copyable type to the config buffer.
        // The sole reinterpret_cast in the entire cd layer lives here.
        template<typename T>
        requires std::is_trivially_copyable_v<T>
        auto append(const T& t) -> descriptor_builder&
        {
            const auto n = sizeof(T);
            if (_cursor + n <= _config_buf.size()) {
                std::memcpy(_config_buf.data() + _cursor,
                            reinterpret_cast<const u8*>(&t), n);
                _cursor += static_cast<u16>(n);
            } else {
                _overflow = true;
            }
            return *this;
        }

        // Append raw bytes from a span — used for runtime-sized descriptors
        // (cs_endpoint, etc.) where the template size is not a compile-time constant.
        auto append_bytes(std::span<const u8> data) -> descriptor_builder&
        {
            if (_cursor + data.size() <= _config_buf.size()) {
                std::memcpy(_config_buf.data() + _cursor, data.data(), data.size());
                _cursor += static_cast<u16>(data.size());
            } else {
                _overflow = true;
            }
            return *this;
        }

        // Allocate the next interface number (monotonically from 0).
        auto alloc_interface() -> u8 { return _iface_count++; }

        // Allocate a free IN endpoint (EP1–EP7).
        // Returns EP_DIR_IN | ep_num, or ep_alloc_error on failure.
        auto alloc_endpoint_in(xfer_type_t /*xfer*/ = xfer_type_t::bulk)
            -> std::expected<u8, ep_alloc_error>
        {
            for (u8 i = 0; i < 7; ++i) {
                if (_ep_in_used & (1u << i)) continue;
                if (_ep_constraints != nullptr) {
                    // ep_matrix constraint check — stubbed pending format definition.
                    // Replace with actual capability lookup once ep_matrix is designed.
                    return std::unexpected(ep_alloc_error::constraint_violation);
                }
                _ep_in_used |= static_cast<u8>(1u << i);
                return static_cast<u8>(ep_dir_in | (i + 1u));
            }
            return std::unexpected(ep_alloc_error::no_free_ep);
        }

        // Allocate a free OUT endpoint (EP1–EP7).
        // Returns the plain endpoint number (no direction bit), or ep_alloc_error on failure.
        auto alloc_endpoint_out(xfer_type_t /*xfer*/ = xfer_type_t::bulk)
            -> std::expected<u8, ep_alloc_error>
        {
            for (u8 i = 0; i < 7; ++i) {
                if (_ep_out_used & (1u << i)) continue;
                if (_ep_constraints != nullptr) {
                    return std::unexpected(ep_alloc_error::constraint_violation);
                }
                _ep_out_used |= static_cast<u8>(1u << i);
                return static_cast<u8>(i + 1u);
            }
            return std::unexpected(ep_alloc_error::no_free_ep);
        }

        // Intern a string. Returns its USB string descriptor index (1-based).
        // Deduplicates: identical byte content returns the existing ID without
        // consuming a new slot.
        // Sparse ID assignment: assigns the lowest positive integer not already in use.
        auto intern(std::string_view content,
                    string_overflow_policy policy = string_overflow_policy::error)
            -> std::expected<u8, string_error>
        {
            const auto raw_len = content.size();
            if (raw_len > MAX_STRING_ENTRY_BYTES && policy == string_overflow_policy::error)
                return std::unexpected(string_error::overflow);

            const u8 copy_len = static_cast<u8>(
                raw_len > MAX_STRING_ENTRY_BYTES ? MAX_STRING_ENTRY_BYTES : raw_len);

            // Deduplication: return existing ID if an identical string is already interned.
            for (const auto& e : _string_pool) {
                if (e.id == 0) continue;
                if (e.length == copy_len
                    && std::memcmp(e.data, content.data(), copy_len) == 0)
                    return e.id;
            }

            // Find a free slot.
            string_entry* slot = nullptr;
            for (auto& e : _string_pool) {
                if (e.id == 0) { slot = &e; break; }
            }
            if (!slot) return std::unexpected(string_error::pool_exhausted);

            // Assign the lowest available ID (sparse-safe: scan for gaps starting at 1).
            u8 next_id = 1;
            bool collision;
            do {
                collision = false;
                for (const auto& e : _string_pool) {
                    if (e.id == next_id) { collision = true; ++next_id; break; }
                }
            } while (collision);

            slot->id     = next_id;
            slot->length = copy_len;
            std::memcpy(slot->data, content.data(), copy_len);
            return next_id;
        }

        // Patch wTotalLength and bNumInterfaces in the configuration descriptor header,
        // then return a span over the populated region of config_buf.
        // The returned span is valid as long as config_buf is alive.
        auto build() -> std::expected<std::span<const u8>, build_error>
        {
            if (_overflow) return std::unexpected(build_error::buffer_overflow);

            // Patch the configuration descriptor header at the start of config_buf.
            // configuration_descriptor (packed) layout:
            //   [0]     bLength
            //   [1]     bDescriptorType
            //   [2..3]  wTotalLength  (u16 LE)  ← write cursor value
            //   [4]     bNumInterfaces           ← write interface count
            auto* buf = _config_buf.data();
            buf[2] = static_cast<u8>(_cursor & 0xFFu);
            buf[3] = static_cast<u8>(_cursor >> 8u);
            buf[4] = _iface_count;

            return std::span<const u8>{ buf, _cursor };
        }

        // Accessors for free functions (set_manufacturer, add_cdc, etc.).
        auto device_desc_ref()    -> device_descriptor&       { return _device_desc; }
        auto string_pool_ref()    -> std::span<string_entry>  { return _string_pool; }
        auto ep_constraints_ptr() -> const ep_matrix*         { return _ep_constraints; }

    private:
        std::span<u8>           _config_buf;
        std::span<string_entry> _string_pool;
        device_descriptor&      _device_desc;
        const ep_matrix*        _ep_constraints;

        u16  _cursor      = 0;
        u8   _iface_count = 0;
        u8   _ep_in_used  = 0; // bitmask bits 0-6: EP1-EP7 IN allocated
        u8   _ep_out_used = 0; // bitmask bits 0-6: EP1-EP7 OUT allocated
        bool _overflow    = false;
    };

    // ========================================================================
    // Device descriptor string helpers — free functions
    // Each calls intern() then writes the returned index into the appropriate
    // device_descriptor string-index field.
    // ========================================================================

    inline auto set_manufacturer(descriptor_builder& b, std::string_view s,
                                  string_overflow_policy pol = string_overflow_policy::error)
        -> std::expected<void, string_error>
    {
        auto r = b.intern(s, pol);
        if (!r) return std::unexpected(r.error());
        b.device_desc_ref().fields.manufacturer_string_index = *r;
        return {};
    }

    inline auto set_product(descriptor_builder& b, std::string_view s,
                             string_overflow_policy pol = string_overflow_policy::error)
        -> std::expected<void, string_error>
    {
        auto r = b.intern(s, pol);
        if (!r) return std::unexpected(r.error());
        b.device_desc_ref().fields.product_string_index = *r;
        return {};
    }

    inline auto set_serial(descriptor_builder& b, std::string_view s,
                            string_overflow_policy pol = string_overflow_policy::error)
        -> std::expected<void, string_error>
    {
        auto r = b.intern(s, pol);
        if (!r) return std::unexpected(r.error());
        b.device_desc_ref().fields.serial_number_string_index = *r;
        return {};
    }

    // ========================================================================
    // add_* config structs and free functions
    // ========================================================================

    struct cdc_config {
        u16              data_ep_size          = 64;
        u16              notification_ep_size  = 16;
        u8               notification_interval = 16; // bInterval for interrupt IN EP, in frames
        std::string_view string                = {}; // interface string; empty = no string
    };

    struct midi_config {
        u8               cables = 1;
        std::string_view string = {};
    };

    struct hid_config {
        std::span<const u8> report_descriptor = {}; // pre-built HID report descriptor bytes
        u16                 ep_size           = 64;
        u8                  interval          = 10;  // bInterval for interrupt IN EP, in ms
        std::string_view    string            = {};
    };

    // -------------------------------------------------------------------------
    // add_cdc — emit a full CDC-ACM function block.
    //
    // Descriptor emission order (per USB CDC 1.2 + IAD ECN, as in TUD_CDC_DESCRIPTOR):
    //   1.  IAD                              (8 bytes)
    //   2.  Communication interface          (9 bytes)
    //   3.  Header Functional                (5 bytes)
    //   4.  Call Management Functional       (5 bytes)
    //   5.  ACM Functional                   (4 bytes)
    //   6.  Union Functional                 (5 bytes)
    //   7.  Interrupt IN endpoint            (7 bytes)  — notification channel
    //   8.  Data interface                   (9 bytes)
    //   9.  Bulk OUT endpoint                (7 bytes)
    //  10.  Bulk IN endpoint                 (7 bytes)
    //
    // Total fixed: 71 bytes per CDC instance.
    // -------------------------------------------------------------------------
    inline auto add_cdc(descriptor_builder& b, cdc_config cfg)
        -> std::expected<void, ep_alloc_error>
    {
        // Allocate all endpoints before appending any bytes — fail fast.
        auto notif_ep = b.alloc_endpoint_in(xfer_type_t::interrupt);
        if (!notif_ep) return std::unexpected(notif_ep.error());

        auto bulk_out = b.alloc_endpoint_out(xfer_type_t::bulk);
        if (!bulk_out) return std::unexpected(bulk_out.error());

        auto bulk_in  = b.alloc_endpoint_in(xfer_type_t::bulk);
        if (!bulk_in)  return std::unexpected(bulk_in.error());

        u8 str_idx = 0;
        if (!cfg.string.empty()) {
            if (auto r = b.intern(cfg.string)) str_idx = *r;
        }

        const u8 comm_itf = b.alloc_interface();
        const u8 data_itf = b.alloc_interface();

        using dc = device_descriptor::device_class;

        // 1. IAD
        b.append(cd::iad_descriptor{.fields = {
            .first_interface   = comm_itf,
            .interface_count   = 2,
            .function_class    = dc::cdc,
            .function_subclass = 0x02, // ACM
            .function_protocol = 0x00, // no class-specific protocol
            .function_string   = str_idx,
        }});

        // 2. Communication (Control) Interface — 1 endpoint (notification IN)
        b.append(cd::interface_descriptor{.fields = {
            .interface_number   = comm_itf,
            .alternate_setting  = 0,
            .num_endpoints      = 1,
            .interface_class    = dc::cdc,
            .interface_subclass = 0x02, // ACM
            .interface_protocol = 0x00,
            .interface_string   = str_idx,
        }});

        // 3–6. CDC functional descriptors
        b.append(cd::cdc::header_functional{});

        b.append(cd::cdc::call_management_functional{.fields = {
            .capabilities  = 0x00,
            .data_interface = data_itf,
        }});

        b.append(cd::cdc::acm_functional{});

        b.append(cd::cdc::union_functional{.fields = {
            .control_interface = comm_itf,
            .data_interface    = data_itf,
        }});

        // 7. Interrupt IN endpoint — notification channel
        b.append(cd::endpoint_descriptor{.fields = {
            .endpoint_address = *notif_ep,
            .attributes       = cd::ep_attr_t{ xfer_type_t::interrupt },
            .max_packet_size  = cfg.notification_ep_size,
            .interval         = cfg.notification_interval,
        }});

        // 8. Data Interface — 2 bulk endpoints
        b.append(cd::interface_descriptor{.fields = {
            .interface_number   = data_itf,
            .alternate_setting  = 0,
            .num_endpoints      = 2,
            .interface_class    = dc::cdc_data,
            .interface_subclass = 0x00,
            .interface_protocol = 0x00,
            .interface_string   = 0,
        }});

        // 9–10. Bulk endpoints (OUT before IN — matches TinyUSB ordering)
        b.append(cd::endpoint_descriptor{.fields = {
            .endpoint_address = *bulk_out,
            .attributes       = cd::ep_attr_t{ xfer_type_t::bulk },
            .max_packet_size  = cfg.data_ep_size,
            .interval         = 0,
        }});

        b.append(cd::endpoint_descriptor{.fields = {
            .endpoint_address = *bulk_in,
            .attributes       = cd::ep_attr_t{ xfer_type_t::bulk },
            .max_packet_size  = cfg.data_ep_size,
            .interval         = 0,
        }});

        return {};
    }

    // -------------------------------------------------------------------------
    // add_midi — emit a full USB MIDI 1.0 function block.
    //
    // Jack ID formula (cable index c, 0-based) — single source of truth:
    //   Embedded IN  = c*4 + 1
    //   External IN  = c*4 + 2
    //   Embedded OUT = c*4 + 3,  source: External IN (c*4+2), pin 1
    //   External OUT = c*4 + 4,  source: Embedded IN (c*4+1), pin 1
    //
    // Descriptor emission order:
    //   1.  Standard AC interface           (9 bytes)
    //   1a. CS AC Interface Header          (9 bytes)
    //   2.  Standard MS interface           (9 bytes)
    //   2a. CS MS Interface Header          (7 bytes)
    //   3.  Per cable × 4 jack descriptors  (6+6+9+9 = 30 bytes × cables)
    //   4.  Bulk OUT endpoint               (7 bytes)
    //   5.  CS MS OUT endpoint              (4 + cables bytes)
    //   6.  Bulk IN endpoint                (7 bytes)
    //   7.  CS MS IN endpoint               (4 + cables bytes)
    // -------------------------------------------------------------------------
    inline auto add_midi(descriptor_builder& b, midi_config cfg)
        -> std::expected<void, ep_alloc_error>
    {
        auto bulk_out = b.alloc_endpoint_out(xfer_type_t::bulk);
        if (!bulk_out) return std::unexpected(bulk_out.error());

        auto bulk_in  = b.alloc_endpoint_in(xfer_type_t::bulk);
        if (!bulk_in)  return std::unexpected(bulk_in.error());

        u8 str_idx = 0;
        if (!cfg.string.empty()) {
            if (auto r = b.intern(cfg.string)) str_idx = *r;
        }

        const u8 ac_itf = b.alloc_interface();
        const u8 ms_itf = b.alloc_interface();

        using dc = device_descriptor::device_class;

        // 1. Standard AC Interface — 0 endpoints (dummy required by MIDI subclass)
        b.append(cd::interface_descriptor{.fields = {
            .interface_number   = ac_itf,
            .alternate_setting  = 0,
            .num_endpoints      = 0,
            .interface_class    = dc::audio,
            .interface_subclass = 0x01, // AUDIO_SUBCLASS_CONTROL
            .interface_protocol = 0x00,
            .interface_string   = str_idx,
        }});

        // 1a. CS AC Interface Header
        b.append(cd::midi::cs_ac_header{.fields = {
            .total_length = 9,    // just this header; no other AC descriptors
            .ms_interface = ms_itf,
        }});

        // 2. Standard MS Interface — 2 bulk endpoints
        b.append(cd::interface_descriptor{.fields = {
            .interface_number   = ms_itf,
            .alternate_setting  = 0,
            .num_endpoints      = 2,
            .interface_class    = dc::audio,
            .interface_subclass = 0x03, // AUDIO_SUBCLASS_MIDI_STREAMING
            .interface_protocol = 0x00,
            .interface_string   = 0,
        }});

        // 2a. CS MS Interface Header
        // wTotalLength = this header (7) + cable block (cables × 30 bytes).
        //   Per cable: in_jack(6) + in_jack(6) + out_jack<1>(9) + out_jack<1>(9) = 30.
        // CS endpoint descriptors that follow the bulk EPs are NOT counted here.
        const u16 ms_cs_total = static_cast<u16>(7u + cfg.cables * 30u);
        b.append(cd::midi::cs_ms_header{.fields = {
            .total_length = ms_cs_total,
        }});

        // 3. Jack descriptors — one group per cable
        for (u8 c = 0; c < cfg.cables; ++c) {
            const u8 emb_in_id  = static_cast<u8>(c * 4u + 1u);
            const u8 ext_in_id  = static_cast<u8>(c * 4u + 2u);
            const u8 emb_out_id = static_cast<u8>(c * 4u + 3u);
            const u8 ext_out_id = static_cast<u8>(c * 4u + 4u);

            // MIDI IN Jack — Embedded
            b.append(cd::midi::in_jack{.fields = {
                .type_       = cd::midi::jack_type::embedded,
                .jack_id     = emb_in_id,
                .jack_string = 0,
            }});

            // MIDI IN Jack — External
            b.append(cd::midi::in_jack{.fields = {
                .type_       = cd::midi::jack_type::external,
                .jack_id     = ext_in_id,
                .jack_string = 0,
            }});

            // MIDI OUT Jack — Embedded (data path: External IN → this jack → hardware)
            {
                cd::midi::out_jack<1> j{};
                j.fields.type_       = cd::midi::jack_type::embedded;
                j.fields.jack_id     = emb_out_id;
                j.fields.pins[0]     = { .source_id = ext_in_id, .source_pin = 1 };
                j.fields.jack_string = 0;
                b.append(j);
            }

            // MIDI OUT Jack — External (data path: Embedded IN → host)
            {
                cd::midi::out_jack<1> j{};
                j.fields.type_       = cd::midi::jack_type::external;
                j.fields.jack_id     = ext_out_id;
                j.fields.pins[0]     = { .source_id = emb_in_id, .source_pin = 1 };
                j.fields.jack_string = 0;
                b.append(j);
            }
        }

        // 4. Bulk OUT endpoint
        b.append(cd::endpoint_descriptor{.fields = {
            .endpoint_address = *bulk_out,
            .attributes       = cd::ep_attr_t{ xfer_type_t::bulk },
            .max_packet_size  = 64,
            .interval         = 0,
        }});

        // 5. CS MS OUT endpoint — lists all Embedded OUT jack IDs (c*4+3)
        // cable count is runtime, so build the descriptor into a stack buffer.
        {
            u8 cs_out[4 + 16] = {};  // max 16 cables
            cs_out[0] = static_cast<u8>(4u + cfg.cables);
            cs_out[1] = static_cast<u8>(cd::desc_type_t::cs_endpoint);
            cs_out[2] = static_cast<u8>(cd::midi::cs_ep_subtype::general);
            cs_out[3] = cfg.cables;
            for (u8 c = 0; c < cfg.cables; ++c)
                cs_out[4 + c] = static_cast<u8>(c * 4u + 3u); // Embedded OUT jack IDs
            b.append_bytes(std::span{cs_out, static_cast<size_t>(4u + cfg.cables)});
        }

        // 6. Bulk IN endpoint
        b.append(cd::endpoint_descriptor{.fields = {
            .endpoint_address = *bulk_in,
            .attributes       = cd::ep_attr_t{ xfer_type_t::bulk },
            .max_packet_size  = 64,
            .interval         = 0,
        }});

        // 7. CS MS IN endpoint — lists all Embedded IN jack IDs (c*4+1)
        {
            u8 cs_in[4 + 16] = {};
            cs_in[0] = static_cast<u8>(4u + cfg.cables);
            cs_in[1] = static_cast<u8>(cd::desc_type_t::cs_endpoint);
            cs_in[2] = static_cast<u8>(cd::midi::cs_ep_subtype::general);
            cs_in[3] = cfg.cables;
            for (u8 c = 0; c < cfg.cables; ++c)
                cs_in[4 + c] = static_cast<u8>(c * 4u + 1u); // Embedded IN jack IDs
            b.append_bytes(std::span{cs_in, static_cast<size_t>(4u + cfg.cables)});
        }

        return {};
    }

    // -------------------------------------------------------------------------
    // add_hid — emit a HID function block.
    // Raw report descriptor span path only; a typed DSL is deferred.
    // The report descriptor bytes themselves are NOT emitted here — they are
    // served separately through the stack's descriptor callback mechanism.
    //
    // Descriptor emission order:
    //   1. Standard HID interface     (9 bytes)
    //   2. HID class descriptor       (9 bytes — single report descriptor entry)
    //   3. Interrupt IN endpoint      (7 bytes)
    // -------------------------------------------------------------------------
    inline auto add_hid(descriptor_builder& b, hid_config cfg)
        -> std::expected<void, ep_alloc_error>
    {
        auto ep_in = b.alloc_endpoint_in(xfer_type_t::interrupt);
        if (!ep_in) return std::unexpected(ep_in.error());

        u8 str_idx = 0;
        if (!cfg.string.empty()) {
            if (auto r = b.intern(cfg.string)) str_idx = *r;
        }

        const u8 itf = b.alloc_interface();

        using dc = device_descriptor::device_class;

        // 1. Standard HID Interface
        b.append(cd::interface_descriptor{.fields = {
            .interface_number   = itf,
            .alternate_setting  = 0,
            .num_endpoints      = 1,
            .interface_class    = dc::hid,
            .interface_subclass = 0x00,
            .interface_protocol = 0x00,
            .interface_string   = str_idx,
        }});

        // 2. HID Class Descriptor (single report descriptor entry)
        b.append(cd::hid::hid_descriptor<1>::make_report(
            static_cast<u16>(cfg.report_descriptor.size())));

        // 3. Interrupt IN endpoint
        b.append(cd::endpoint_descriptor{.fields = {
            .endpoint_address = *ep_in,
            .attributes       = cd::ep_attr_t{ xfer_type_t::interrupt },
            .max_packet_size  = cfg.ep_size,
            .interval         = cfg.interval,
        }});

        return {};
    }

} // namespace lm::usb