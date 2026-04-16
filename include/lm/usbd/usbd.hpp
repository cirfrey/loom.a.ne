#pragma once

#include "lm/core/types.hpp"

#include <span>
#include <array>

/// Google AI:
/// TinyUSB in ESP-IDF is semi-static. While we can generate descriptors dynamically, the number of interfaces and endpoints must be set to their maximum in menuconfig
/// so the stack allocates enough memory at startup.
namespace lm::usbd
{
    struct endpoint_info_t
    {
        // NOTE: In USB-speak: IN always means "Into the Computer."
        enum class type_t
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

        enum class interface_type_t {
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

        type_t in;
        u8 in_itf_idx = 0; // What interface is using this endpoint.
        interface_type_t in_itf = interface_type_t::unassigned;

        type_t out;
        u8 out_itf_idx = 0;
        interface_type_t out_itf = interface_type_t::unassigned;
    };

    namespace debug
    {
        /**
         * Table Layout Constants (Documentation Only):
         * - Row Width: 84 chars + '\n' = 85 bytes
         * - Static Rows: 3 separators + 1 header = 4 rows
         * - Data Rows: 85 * ArrSize
         * - Null Terminator: 1 byte
         */
        constexpr auto ept_row_width = 85;
        template <u32 ArrSize>
        constexpr st ep_table_size = (ept_row_width * 4) + (ept_row_width * ArrSize) + 1;

        // +----+-------------------+-----+------------------+-----------+----------------------+
        // Returns a printable version of the eps in a nice table, good for debugging.
        inline auto eps_to_table(std::span<endpoint_info_t> eps, mut_text in_buf, text prefix) -> mut_text;
    } // namespace lm::usbd::debug

    struct descriptors_t
    {
        // --- Device descriptors ---
        u16 vendor_id  = 0x303A; // The primary USB Vendor ID (VID) for Espressif Systems.
        u16 product_id = 0x80C4; // Lolin S2 Mini - UF2 Bootloader
        u16 bcd_device = 0x0100; // Device Release Number (1.0)

        // --- String descriptors ---
        const char* manufacturer = "Cirfrey Inc.";
        const char* product      = "loom.a.ne Multidevice";
        const char* midi         = "loom.a.ne MIDI";
        const char* hid          = "loom.a.ne HID";
        const char* uac          = "loom.a.ne UAC";
        const char* cdc          = "loom.a.ne CDC";
        const char* msc          = "loom.a.ne MSC";
    };

    struct configuration_t
    {
        bool cdc; // True = 3 EP. Full CDC support.
                  // False = Can still log with HID vendor logging if HID is enabled.
        bool cdc_strict_endpoints = false;

        enum uac_t : u8 {
            no_uac,             // 0 EP.
            speaker,            // 2 EP.
            microphone,         // 1 EP.
            speaker_microphone  // 3 EP.
        } uac;
        u8 speaker_channels = 0;
        u8 microphone_channels = 0;

        // Enables/disables Keyboard/Mouse/Gamepad.
        // NOTE: logging = logging_t::hid forces this to true.
        // Look at lm::usbd::hid::hid_reportid for the reportids
        // of each hid type.
        bool hid;
        u8 hid_pooling_interval_ms = 5;

        enum midi_t : u8 {
            no_midi,   // 0 EP.
            midi_in,   // 1 EP.
            midi_out,  // 1 EP.
            midi_inout // 2 EP.
        } midi;
        u8 midi_cable_count = 4;
        static const u8 midi_max_cable_count = 16;
        bool midi_strict_endpoints = false;

        bool msc; // 2 EP.
        bool msc_strict_endpoints = false;
    };

    auto init(
        configuration_t& cfg,
        std::span<endpoint_info_t> endpoints,
        descriptors_t descriptors = {}
    ) -> void;

    enum class event : u8
    {
        // Requests for usbd to reply with the stuff below, as appropriate.
        // Don't flood the usbd strand with these requests.
        get_status,

        cdc_enabled,
        cdc_disabled,
        hid_enabled,
        hid_disabled,
    };
}


/// Impls.

#include "lm/core/reflect.hpp"
#include "lm/core/math.hpp"
#include <cstdio>

inline auto lm::usbd::debug::eps_to_table(std::span<endpoint_info_t> eps, mut_text in_buf, text prefix) -> mut_text
{
    auto out_buf = mut_text{in_buf.data, 0};

    const char* sep = "+----+--------------------------------------+--------------------------------------+\n";

    out_buf.size += std::snprintf(
        out_buf.data + out_buf.size, in_buf.size - out_buf.size,
        "%.*s%s", (int)prefix.size, prefix.data, sep
    );
    out_buf.size = clamp(out_buf.size, 0, in_buf.size);
    out_buf.size += std::snprintf(
        out_buf.data + out_buf.size, in_buf.size - out_buf.size,
        "%.*s| EP | (itf id:itf type ) IN                | (itf id:itf type ) OUT               |\n",
        (int)prefix.size, prefix.data
    );
    out_buf.size = clamp(out_buf.size, 0, in_buf.size);
    out_buf.size += std::snprintf(
        out_buf.data + out_buf.size, in_buf.size - out_buf.size,
        "%.*s%s", (int)prefix.size, prefix.data, sep
    );
    out_buf.size = clamp(out_buf.size, 0, in_buf.size);

    for (size_t i = 0; i < eps.size(); ++i) {
        const auto& ep = eps[i];

        using re_ept_t = renum<endpoint_info_t::type_t, 0, 20>;
        using re_itf_t = renum<endpoint_info_t::interface_type_t, 0, 10>;
        auto in      = re_ept_t::unqualified(ep.in);
        auto in_itf  = re_itf_t::unqualified(ep.in_itf);
        auto out     = re_ept_t::unqualified(ep.out);
        auto out_itf = re_itf_t::unqualified(ep.out_itf);

        out_buf.size += std::snprintf(
            out_buf.data + out_buf.size, in_buf.size - out_buf.size,
            "%.*s|  %zu | (%u:%-14.*s) %-17.*s | (%u:%-14.*s) %-17.*s |\n",
            (int)prefix.size, prefix.data,
            i,
            ep.in_itf_idx,  (int)in_itf.size,  in_itf.data,  (int)in.size, in.data,
            ep.out_itf_idx, (int)out_itf.size, out_itf.data, (int)out.size, out.data
        );
        out_buf.size = clamp(out_buf.size, 0, in_buf.size);
    }

    out_buf.size += std::snprintf(
        out_buf.data + out_buf.size, in_buf.size - out_buf.size,
        "%.*s%s", (int)prefix.size, prefix.data, sep
    );
    out_buf.size = clamp(out_buf.size, 0, in_buf.size);
    return out_buf;
}
