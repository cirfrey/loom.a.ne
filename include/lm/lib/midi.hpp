#pragma once

#include "lm/core/math.hpp"

#include "lm/config.hpp"

#include <bitset>

namespace lm::midi
{
    using backend = config_t::midi_t::backend_t::id::id_t;

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
        constexpr header_byte(cin c, u8 cab) : code(c), cable(clip_bits_after<4>(cab)) {}
    };
    struct status_byte {
        u8      channel : 4;
        status  type    : 4;
        constexpr status_byte(status s, u8 chan) : channel(clip_bits_after<4>(chan)), type(s) {}
    };
    struct data_byte {
        u8 value : 7;
        u8 msb   : 1 = 0; // Always 0 for MIDI data
        constexpr data_byte(u8 v, bool m = false) : value(clip_bits_after<7>(v)), msb(m) {}
    };
    // The 4-Byte Midi packet.
    struct alignas(u32) packet {
        header_byte header;
        status_byte status;
        data_byte   note;
        data_byte   velocity;

        struct constructor {
            u8  cable = 0;
            cin code;
            u8  channel = 0;
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

    struct payload
    {
        std::bitset<backend::count> backend = 0;
        packet pkt;

        static constexpr auto note_on(packet::constructor c) -> payload
        { c.code = cin::note_on; c.type = status::note_on; return payload(0, c); }

        auto to(std::initializer_list<midi::backend> target_backends) -> payload&
        { backend.reset() ; for(auto b : target_backends) backend.set(b); return *this; }
    };
}
