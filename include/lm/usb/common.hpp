#pragma once

#include "lm/core/types.hpp"

#include <span>

// NOTE: In USB-speak: IN always means "Into the Computer."
namespace lm::usb
{
    enum class ept_t
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

    enum class itf_t
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

    struct ep_t
    {
        ept_t in      = ept_t::unassigned;
        u8 in_itf_idx = 0; // What interface is using this endpoint.
        itf_t in_itf  = itf_t::unassigned;

        ept_t out      = ept_t::unassigned;
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

    // Thigies for low-level usb bitbangin' n stuff.
    namespace standard_setup_request { enum standard_setup_request_t {
        get_status        = 0x00, // Returns status for a device, interface, or endpoint.
        clear_feature     = 0x01, // Clears a specific feature (e.g., halting an endpoint).
        set_feature       = 0x03, // Enables a specific feature (e.g., remote wakeup).
        set_address       = 0x05, // Assigns a unique address to the device during enumeration.
        get_descriptor    = 0x06, // Requests device descriptors (Device, Configuration, String).
        set_descriptor    = 0x07, // Updates existing descriptors.
        get_configuration = 0x08, // Returns the current device configuration value.
        set_configuration = 0x09, // Sets the device configuration.
        get_interface     = 0x0a, // Returns the selected alternate setting for an interface.
        set_interface     = 0x0b, // Allows the host to select an alternate setting for an interface.
        synch_frame       = 0x0c, // Used for isochronous endpoints to synchronize frame patterns.
    }; }
}
