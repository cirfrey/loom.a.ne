#pragma once

#include "lm/core/math.hpp"

#include "lm/tasks/usbd.hpp"
#include "lm/usbd/common.hpp"

namespace lm::usbd::midi
{
    auto do_configuration_descriptor(
        configuration_descriptor_builder_state_t& s,
        cfg_t& cfg,
        std::span<ep_t> eps
    ) -> void;

    enum class cin : u8 {
        note_off      = 0x08,
        note_on       = 0x09,
        poly_after    = 0x0A,
        control_chg   = 0x0B,
        program_chg   = 0x0C,
        channel_after = 0x0D,
        pitch_bend    = 0x0E,
        system_common = 0x0F
    };
    enum class status : u8 {
        note_off = 0x8,
        note_on  = 0x9,
        // ... higher nibble of the MIDI status byte
    };
    struct header_byte {
        cin code  : 4;
        u8  cable : 4;
        constexpr header_byte(cin c, u8 cab) : code(c), cable(clip_bits<4>(cab)) {}
    };
    struct status_byte {
        u8      channel : 4;
        status  type    : 4;
        constexpr status_byte(status s, u8 chan) : channel(clip_bits<4>(chan)), type(s) {}
    };
    struct data_byte {
        u8 value : 7;
        u8 msb   : 1 = 0; // Always 0 for MIDI data
        constexpr data_byte(u8 v, bool m = false) : value(clip_bits<7>(v)), msb(m) {}
    };
    // The 4-Byte Midi packet.
    struct alignas(u32) packet {
        header_byte header;
        status_byte status;
        data_byte   note;
        data_byte   velocity;

        struct constructor {
            u8  cable;
            cin code;
            u8  channel;
            midi::status type;
            u8 note;
            u8 velocity;
            bool note_msb = 0;
            bool velocity_msb = 0;
        };
        constexpr packet(constructor c)
            : header(c.code, c.cable)
            , status(c.type, c.channel)
            , note(c.note, c.note_msb)
            , velocity(c.velocity, c.velocity_msb)
        {}
    };
    static_assert(sizeof(packet) == 4, "The midi packet must be 4 bytes.");
}
