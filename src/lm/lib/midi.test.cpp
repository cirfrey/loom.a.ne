#include "lm/test.hpp"
#include "lm/lib/midi.hpp"

// USB MIDI packet layout: [header][status][data1][data2]
// header = (cable << 4) | CIN
// CIN for note_on = 0x09
// So cable=0, note_on: header = 0x09
// So cable=1, note_on: header = 0x19 — NOT 0x91
// This is the classic encoding bug. This test catches it.

#if 0
LM_TEST_SUITE("lib.midi.packet",
{
    // Cable 0, Note On
    {
        auto pkt = lm::usbd::midi::make_packet(
            /*cable=*/0,
            lm::usbd::midi::cin::note_on,
            /*status=*/0x90,
            /*data1=*/60,
            /*data2=*/100);

        check(pkt.bytes[0] == 0x09u, "cable 0 note_on: header == 0x09");
        check(pkt.bytes[1] == 0x90u, "cable 0 note_on: status byte");
        check(pkt.bytes[2] == 60u,   "cable 0 note_on: note number");
        check(pkt.bytes[3] == 100u,  "cable 0 note_on: velocity");
    }

    // Cable 1, Note On — the critical test
    // Header must be 0x19, not 0x91
    {
        auto pkt = lm::usbd::midi::make_packet(
            /*cable=*/1,
            lm::usbd::midi::cin::note_on,
            /*status=*/0x90,
            /*data1=*/60,
            /*data2=*/100);

        check(pkt.bytes[0] == 0x19u,
              "cable 1 note_on: header == 0x19, NOT 0x91");
    }

    // Cable 0, Note Off
    {
        auto pkt = lm::usbd::midi::make_packet(
            /*cable=*/0,
            lm::usbd::midi::cin::note_off,
            /*status=*/0x80,
            /*data1=*/60,
            /*data2=*/0);

        check(pkt.bytes[0] == 0x08u, "cable 0 note_off: header == 0x08");
        check(pkt.bytes[1] == 0x80u, "cable 0 note_off: status byte");
    }

    // Packet size invariant
    {
        auto pkt = lm::usbd::midi::make_packet(0, lm::usbd::midi::cin::note_on,
                                                0x90, 0, 0);
        static_assert(sizeof(pkt) == 4, "USB MIDI packet must be exactly 4 bytes");
        check(true, "sizeof(midi_packet) == 4 (static_assert)");
    }
});
#endif
