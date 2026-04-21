#include "lm/test.hpp"
#include "lm/lib/midi.hpp"

// USB MIDI packet layout: [header][status][data1][data2]
// header = (cable << 4) | CIN
// CIN for note_on = 0x09
// So cable=0, note_on: header = 0x09
// So cable=1, note_on: header = 0x19 — NOT 0x91
// This is the classic encoding bug. This test catches it.

using namespace lm;

LM_TEST_SUITE("lib.midi.packet",
{
    // Cable 0, Note On
    {
        auto pkt = midi::packet({
            .cable = 0,
            .code = midi::cin::note_on,
            .type = midi::status::note_on,
            .note = 60,
            .velocity = 100
        });
        u8* bytes = (u8*)&pkt;

        check(bytes[0] == 0x09u, "cable 0 note_on: header == 0x09");
        check(bytes[1] == 0x90u, "cable 0 note_on: status byte");
        check(bytes[2] == 60u,   "cable 0 note_on: note number");
        check(bytes[3] == 100u,  "cable 0 note_on: velocity");
    }

    // Cable 1, Note On — the critical test
    // Header must be 0x19, not 0x91
    {
        auto pkt = midi::packet({
            .cable = 1,
            .code = midi::cin::note_on,
            .type = midi::status::note_on,
            .note = 60,
            .velocity = 100
        });
        u8* bytes = (u8*)&pkt;
        check(bytes[0] == 0x19u,
              "cable 1 note_on: header == 0x19, NOT 0x91");
    }

    // Cable 0, Note Off
    {
        auto pkt = midi::packet({
            .cable = 0,
            .code = midi::cin::note_off,
            .type = midi::status::note_off,
            .note = 60,
            .velocity = 0
        });
        u8* bytes = (u8*)&pkt;

        check(bytes[0] == 0x08u, "cable 0 note_off: header == 0x08");
        check(bytes[1] == 0x80u, "cable 0 note_off: status byte");
    }
});
