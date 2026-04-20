#include "lm/test.hpp"
#include "lm/core/endian.hpp"

LM_TEST_SUITE("endian.roundtrip",
{
    // u16 roundtrip
    check(lm::ntoh16(lm::hton16(0x1234u)) == 0x1234u, "u16 roundtrip");
    check(lm::ntoh16(lm::hton16(0x0000u)) == 0x0000u, "u16 zero roundtrip");
    check(lm::ntoh16(lm::hton16(0xFFFFu)) == 0xFFFFu, "u16 max roundtrip");

    // u32 roundtrip
    check(lm::ntoh32(lm::hton32(0x12345678u)) == 0x12345678u, "u32 roundtrip");
    check(lm::ntoh32(lm::hton32(0x00000000u)) == 0x00000000u, "u32 zero roundtrip");
    check(lm::ntoh32(lm::hton32(0xFFFFFFFFu)) == 0xFFFFFFFFu, "u32 max roundtrip");
});

LM_TEST_SUITE("endian.byte_order",
{
    // On little-endian (x86, Xtensa in LE mode): hton swaps bytes
    // On big-endian: hton is identity
    // Either way, the network representation of 0x0102 must have 0x01 first
    lm::u16 val = lm::hton16(0x0102u);
    auto* bytes  = reinterpret_cast<lm::u8*>(&val);
    check(bytes[0] == 0x01u, "hton16: high byte first on wire");
    check(bytes[1] == 0x02u, "hton16: low byte second on wire");

    lm::u32 val32 = lm::hton32(0x01020304u);
    auto* b32     = reinterpret_cast<lm::u8*>(&val32);
    check(b32[0] == 0x01u, "hton32: byte 0 is MSB");
    check(b32[3] == 0x04u, "hton32: byte 3 is LSB");
});
