#pragma once

#include "espy/aliases.hpp"

#include <span>

/// Google AI:
/// TinyUSB in ESP-IDF is semi-static. While we can generate descriptors dynamically, the number of interfaces and endpoints must be set to their maximum in menuconfig
/// so the stack allocates enough memory at startup.
namespace espy::usb
{
    struct endpoint_info_t
    {
        // NOTE: In USB-speak: IN always means "Into the Computer."
        enum class type_t
        {
            unassigned, // Mark this port as currently unused.

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
                               /// - The Result: You save 1 endpoint in your limited pool of 6.

            uac2_iso_in,  // Speaker connection.    [Host -> Device].
            uac2_iso_out, // Microphone connection. [Device -> Host].
            uac2_feedback, // Device -> Host.
                           /// Google AI:
                           /// This is a tiny packet sent back to the computer to synchronize the clock. It tells the PC to speed up or slow down the audio stream to prevent "pops" or "clicks."

            // Each midi endpoint has 16 virtual cables/channels.
            midi_bulk_in,  // Send notes to PC. [Device -> Host].
            midi_bulk_out, // Receive notes from PC. [Host -> Device].
        } type;
        u8 interface; // What interface is using this endpoint.
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

        } interface_type;
        enum class direction_t { NONE, IN, OUT, INOUT };
        const direction_t available_directions;
        direction_t configured_direction;
    };

    struct string_descriptor_t
    {
        const char* manufacturer = "Cirfrey Inc.";
        const char* product      = "Espy Multidevice";
        const char* midi         = "ESPY MIDI";
        const char* hid          = "ESPY HID";
        const char* uac          = "ESPY UAC";
        const char* cdc          = "ESPY CDC";
    };

    struct configuration_t
    {
        bool cdc; // True = 3 EP. Full CDC support.
                  // False = Can still log with HID vendor logging if HID is enabled.

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
        // Look at espy::usb::hid_reportid for the reportids of each
        // hid type.
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
    };

    enum class hid_reportid : u8
    {
        mouse    = 1,
        keyboard = 2,
        gamepad  = 3, // Standard 16-button, 2-joystick, 8-way hat gamepad.
        vendor   = 4  // See configuration_t::logging_t::hid_vendor_logging.
    };

    auto init(
        configuration_t& cfg,
        std::span<endpoint_info_t> available_endpoints,
        string_descriptor_t string_descriptor = {}
    ) -> void;
}
