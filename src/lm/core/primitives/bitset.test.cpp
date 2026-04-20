#include "lm/test.hpp"
#include "lm/core/primitives/bitset.hpp"

// ── Layout invariants ─────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.layout",
{
    // Bucket count math
    static_assert(lm::bitset<1>::num_buckets   == 1);
    static_assert(lm::bitset<32>::num_buckets  == 1);
    static_assert(lm::bitset<33>::num_buckets  == 2);
    static_assert(lm::bitset<64>::num_buckets  == 2);
    static_assert(lm::bitset<256>::num_buckets == 8);
    check(true, "bucket count static_asserts passed");

    // Size — 8 buckets of u32 = 32 bytes for bitset<256>
    static_assert(sizeof(lm::bitset<256>) == 32);
    check(true, "sizeof(bitset<256>) == 32");

    // Zero-init — all bits clear on construction
    lm::bitset<64> bs{};
    check(bs.none(),    "default constructed: none()");
    check(!bs.any(),    "default constructed: !any()");
    check(!bs.all(),    "default constructed: !all()");
    check(bs.count() == 0, "default constructed: count() == 0");
});

// ── set / clear / test / flip ─────────────────────────────────────────────────

LM_TEST_SUITE("bitset.single_bit",
{
    lm::bitset<256> bs{};

    // Set and test
    bs.set(0);
    check(bs.test(0),   "set(0): test(0) true");
    check(!bs.test(1),  "set(0): test(1) false");
    check(bs.count() == 1, "set(0): count == 1");

    // Bit at bucket boundary
    bs.set(31);
    check(bs.test(31),  "set(31): test(31) true");
    check(bs.count() == 2, "set(0,31): count == 2");

    // First bit of second bucket
    bs.set(32);
    check(bs.test(32),  "set(32): test(32) true");
    check(!bs.test(33), "set(32): test(33) false");

    // Last valid bit
    bs.set(255);
    check(bs.test(255), "set(255): test(255) true");

    // Clear
    bs.clear(0);
    check(!bs.test(0),  "clear(0): test(0) false");
    check(bs.test(31),  "clear(0): test(31) still true");

    // Flip
    bs.flip(0);
    check(bs.test(0),   "flip(0) on clear: now set");
    bs.flip(0);
    check(!bs.test(0),  "flip(0) on set: now clear");

    // operator[]
    check(bs[31],       "operator[31] matches test(31)");
    check(!bs[0],       "operator[0] matches test(0)");
});

// ── Tail bit isolation — bits beyond N must never be set ─────────────────────
//
// This is the most dangerous invariant. If trim_tail() is wrong, operations
// like all(), count(), and flip_all() produce incorrect results silently.

LM_TEST_SUITE("bitset.tail_isolation",
{
    // bitset<33> has one partial bucket: bucket[1] has 1 valid bit (bit 32)
    // and 31 garbage bits that must stay zero.

    lm::bitset<33> bs{};
    bs.set_all();
    check(bs.all(),     "set_all: all() true");
    check(bs.test(32),  "set_all: bit 32 set");
    check(bs.count() == 33, "set_all: count == 33, not 64");

    bs.flip_all();
    check(bs.none(),    "flip_all after set_all: none()");
    check(bs.count() == 0, "flip_all: count == 0, tail not leaked");

    // Directly poke the raw bucket — simulate a bug writing tail bits
    bs.buckets[1] = 0xFFFFFFFF;   // all 32 bits set, only bit 0 (= bit 32) is valid
    check(bs.count() == 1,        "raw tail poke: count still 1 after trim");
    // Note: count() doesn't trim — it counts raw popcount. This test documents
    // that trim_tail is NOT called by count(). Callers of set/clear/flip don't
    // need to worry because those operations target specific bits. Bulk ops do trim.
    // If this behaviour changes, update this test.
    check(bs.test(32),            "raw tail poke: bit 32 readable");
});

// ── set_all / clear_all / flip_all ───────────────────────────────────────────

LM_TEST_SUITE("bitset.bulk",
{
    {
        lm::bitset<64> bs{};
        bs.set_all();
        check(bs.all(),             "set_all: all()");
        check(bs.count() == 64,     "set_all: count == 64");
        check(bs.test(0),           "set_all: bit 0 set");
        check(bs.test(63),          "set_all: bit 63 set");

        bs.clear_all();
        check(bs.none(),            "clear_all after set_all: none()");
        check(bs.count() == 0,      "clear_all: count == 0");
    }

    {
        lm::bitset<256> bs{};
        bs.set(7);
        bs.flip_all();
        check(!bs.test(7),          "flip_all: previously set bit now clear");
        check(bs.test(0),           "flip_all: previously clear bit now set");
        check(bs.test(255),         "flip_all: last bit set");
        check(bs.count() == 255,    "flip_all: count == 255");
    }
});

// ── find_first_set ────────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.find_first_set",
{
    lm::bitset<256> bs{};

    // Empty — returns npos
    check(bs.find_first_set() == lm::bitset<256>::npos,
          "empty: find_first_set == npos");

    // Bit 0
    bs.set(0);
    check(bs.find_first_set() == 0,     "bit 0: find_first_set == 0");

    // Bit at bucket boundary
    bs.clear(0);
    bs.set(31);
    check(bs.find_first_set() == 31,    "bit 31: find_first_set == 31");

    // First bit of second bucket
    bs.set(32);
    check(bs.find_first_set() == 31,    "bits 31,32: find_first_set == 31");

    // from= parameter
    check(bs.find_first_set(32) == 32,  "from=32: find_first_set == 32");
    check(bs.find_first_set(33) == lm::bitset<256>::npos,
          "from=33: no more set bits → npos");

    // Last bit
    bs.clear_all();
    bs.set(255);
    check(bs.find_first_set() == 255,   "bit 255: find_first_set == 255");
    check(bs.find_first_set(255) == 255, "from=255, bit 255 set: == 255");
    check(bs.find_first_set(254) == 255, "from=254, bit 255 set: == 255");

    // from= beyond all set bits
    check(bs.find_first_set(0) == 255,  "from=0: finds bit 255");
    bs.clear(255);
    check(bs.find_first_set(0) == lm::bitset<256>::npos,
          "after clear: npos");
});

// ── find_first_clear ──────────────────────────────────────────────────────────
// This is the hot path for id_registry::get_fresh(). Test it hard.

LM_TEST_SUITE("bitset.find_first_clear",
{
    {
        // Fully clear — bit 0
        lm::bitset<256> bs{};
        check(bs.find_first_clear() == 0,   "empty: find_first_clear == 0");
    }

    {
        // Fully set — npos
        lm::bitset<256> bs{};
        bs.set_all();
        check(bs.find_first_clear() == lm::bitset<256>::npos,
              "full: find_first_clear == npos");
    }

    {
        // First bucket full, second has a clear bit at position 32
        lm::bitset<256> bs{};
        bs.set_all();
        bs.clear(32);
        check(bs.find_first_clear() == 32,  "first clear at 32: == 32");
        check(bs.find_first_clear(33) == lm::bitset<256>::npos,
              "from=33 with only bit 32 clear: npos");
    }

    {
        // from= skips ahead correctly
        lm::bitset<256> bs{};
        bs.set(0);
        check(bs.find_first_clear(0) == 1,  "from=0, bit 0 set: first clear is 1");
        check(bs.find_first_clear(1) == 1,  "from=1: first clear is 1");
    }

    {
        // Simulates id_registry: user range [32, 254], all framework IDs taken
        lm::bitset<256> bs{};
        for(lm::st i = 0; i < 32; ++i) bs.set(i);   // reserve framework range

        auto first = bs.find_first_clear(32);
        check(first == 32, "id_registry sim: first user id is 32");

        // Fill up to 100, next free should be 101
        for(lm::st i = 32; i <= 100; ++i) bs.set(i);
        check(bs.find_first_clear(32) == 101, "id_registry sim: next free after 100 is 101");
    }

    {
        // Clear bit exactly at bucket boundary
        lm::bitset<256> bs{};
        bs.set_all();
        bs.clear(63);
        check(bs.find_first_clear() == 63,  "clear at 63 (boundary): == 63");

        bs.set(63);
        bs.clear(64);
        check(bs.find_first_clear() == 64,  "clear at 64 (next bucket start): == 64");
    }
});

// ── find_last_set / find_last_clear ───────────────────────────────────────────

LM_TEST_SUITE("bitset.find_last",
{
    lm::bitset<256> bs{};

    check(bs.find_last_set() == lm::bitset<256>::npos, "empty: find_last_set == npos");
    check(bs.find_last_clear() == 255,                 "empty: find_last_clear == 255");

    bs.set(0);
    bs.set(128);
    check(bs.find_last_set()  == 128, "bits 0,128: find_last_set == 128");
    check(bs.find_last_clear() == 255, "bits 0,128: find_last_clear == 255");

    bs.set_all();
    check(bs.find_last_set()  == 255, "full: find_last_set == 255");
    check(bs.find_last_clear() == lm::bitset<256>::npos, "full: find_last_clear == npos");

    bs.clear(255);
    check(bs.find_last_set()  == 254, "clear 255: find_last_set == 254");
    check(bs.find_last_clear() == 255, "clear 255: find_last_clear == 255");
});

// ── count_range ───────────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.count_range",
{
    lm::bitset<256> bs{};
    bs.set(5);
    bs.set(10);
    bs.set(63);
    bs.set(64);
    bs.set(200);

    check(bs.count_range(0,  64)  == 3, "range [0,64):  bits 5,10,63");
    check(bs.count_range(64, 128) == 1, "range [64,128): bit 64");
    check(bs.count_range(0,  256) == 5, "range [0,256): all 5 bits");
    check(bs.count_range(0,  5)   == 0, "range [0,5):   empty");
    check(bs.count_range(5,  6)   == 1, "range [5,6):   bit 5");
    check(bs.count_range(11, 200) == 2, "range [11,200): bits 63,64");
});

// ── slice read ────────────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.slice_read",
{
    lm::bitset<256> bs{};

    // Single-bucket slice — simple case
    bs.buckets[0] = 0xDEADBEEF;
    check(bs.slice<0, 32>() == 0xDEADBEEFu, "slice<0,32>: full bucket");
    check(bs.slice<0,  8>() == 0xEFu,       "slice<0,8>:  low byte");
    check(bs.slice<8,  8>() == 0xBEu,       "slice<8,8>:  second byte");
    check(bs.slice<24, 8>() == 0xDEu,       "slice<24,8>: high byte");

    // Return type selection
    static_assert(std::is_same_v<decltype(bs.slice<0, 8>()),  lm::u8>,  "8-bit → u8");
    static_assert(std::is_same_v<decltype(bs.slice<0, 16>()), lm::u16>, "16-bit → u16");
    static_assert(std::is_same_v<decltype(bs.slice<0, 32>()), lm::u32>, "32-bit → u32");
    static_assert(std::is_same_v<decltype(bs.slice<0, 64>()), lm::u64>, "64-bit → u64");
    check(true, "return type static_asserts passed");

    // Cross-bucket slice — the interesting case
    bs.clear_all();
    bs.buckets[0] = 0xFFFFFFFF;
    bs.buckets[1] = 0x00000000;
    // slice<28, 8>: bits 28-35 — 4 from bucket 0 (0xF), 4 from bucket 1 (0x0)
    check(bs.slice<28, 8>() == 0x0Fu, "cross-bucket slice<28,8>: 0x0F");

    bs.buckets[1] = 0xFFFFFFFF;
    check(bs.slice<28, 8>() == 0xFFu, "cross-bucket slice<28,8> full: 0xFF");

    // Known pattern
    bs.clear_all();
    bs.set(31);
    bs.set(32);
    // slice<30, 4>: bits 30,31,32,33 → 0b0110 = 0x06
    check(bs.slice<30, 4>() == 0x06u, "cross-bucket slice<30,4>: 0x06");
});

// ── slice write ───────────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.slice_write",
{
    {
        // Single bucket, no cross
        lm::bitset<64> bs{};
        bs.set_slice<0, 8>(lm::u8{0xAB});
        check(bs.slice<0, 8>() == 0xABu, "set_slice<0,8>: readback 0xAB");
        check(bs.slice<8, 8>() == 0x00u, "set_slice<0,8>: neighbour unchanged");
    }

    {
        // Roundtrip: write then read
        lm::bitset<256> bs{};
        bs.set_slice<32, 8>(lm::u8{0x7F});
        check(bs.slice<32, 8>() == 0x7Fu,  "roundtrip<32,8>: 0x7F");
        check(bs.slice<0,  32>() == 0x00u,  "roundtrip<32,8>: low bucket untouched");
        check(bs.slice<40, 24>() == 0x00u,  "roundtrip<32,8>: high bits untouched");
    }

    {
        // Cross-bucket write
        lm::bitset<64> bs{};
        // Write 0xFF across the bucket boundary: bits 28-35
        bs.set_slice<28, 8>(lm::u8{0xFF});
        check(bs.slice<28, 8>() == 0xFFu, "cross-bucket set_slice<28,8>: readback");
        // Bits outside the written range must be zero
        check(bs.slice<0,  28>() == 0x00000000u, "cross-bucket: low bits clean");
        check(bs.slice<36, 28>() == 0x00000000u, "cross-bucket: high bits clean");
    }

    {
        // Partial overwrite doesn't disturb existing bits
        lm::bitset<64> bs{};
        bs.set_all();                            // all 1s
        bs.set_slice<8, 8>(lm::u8{0x00});       // clear bits [8,16)
        check(bs.slice<0,  8>() == 0xFFu,  "partial overwrite: low byte intact");
        check(bs.slice<8,  8>() == 0x00u,  "partial overwrite: target cleared");
        check(bs.slice<16, 8>() == 0xFFu,  "partial overwrite: high byte intact");
    }
});

// ── shift operators ───────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.shift",
{
    {
        lm::bitset<64> bs{};
        bs.set(0);
        bs <<= 1;
        check(!bs.test(0),  "<<= 1: bit 0 cleared");
        check(bs.test(1),   "<<= 1: bit 1 set");

        bs <<= 31;
        check(bs.test(32),  "<<= 31: bit 32 set (crossed bucket)");
        check(!bs.test(31), "<<= 31: bit 31 clear");
    }

    {
        lm::bitset<64> bs{};
        bs.set(63);
        bs >>= 1;
        check(!bs.test(63), ">>= 1: bit 63 cleared");
        check(bs.test(62),  ">>= 1: bit 62 set");
    }

    {
        // Shift by full width clears everything
        lm::bitset<64> bs{};
        bs.set_all();
        bs <<= 64;
        check(bs.none(), "<<= N: all cleared");

        bs.set_all();
        bs >>= 64;
        check(bs.none(), ">>= N: all cleared");
    }

    {
        // Shift preserves tail isolation
        lm::bitset<33> bs{};
        bs.set(32);         // only valid bit in second bucket
        bs <<= 1;           // shifts out of range — should disappear
        check(bs.none(), "shift out of range: tail isolated");
    }
});

// ── bitwise operators ─────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.bitwise",
{
    lm::bitset<64> a{}, b{};
    a.set(0); a.set(1); a.set(2);  // 0b111
    b.set(1); b.set(2); b.set(3);  // 0b1110

    auto and_ = a & b;
    check(and_.test(1) && and_.test(2),  "a&b: bits 1,2 set");
    check(!and_.test(0) && !and_.test(3), "a&b: bits 0,3 clear");

    auto or_  = a | b;
    check(or_.test(0) && or_.test(1) && or_.test(2) && or_.test(3),
          "a|b: bits 0-3 set");

    auto xor_ = a ^ b;
    check(xor_.test(0) && xor_.test(3), "a^b: bits 0,3 set (differ)");
    check(!xor_.test(1) && !xor_.test(2), "a^b: bits 1,2 clear (same)");

    auto not_ = ~a;
    check(!not_.test(0) && !not_.test(1) && !not_.test(2), "~a: low bits clear");
    check(not_.test(3), "~a: bit 3 set");
    check(not_.count() == 61, "~a: count == 61");   // 64 - 3
});

// ── equality ──────────────────────────────────────────────────────────────────

LM_TEST_SUITE("bitset.equality",
{
    lm::bitset<64> a{}, b{};
    check(a == b, "two empty bitsets equal");

    a.set(5);
    check(!(a == b), "after set: not equal");

    b.set(5);
    check(a == b, "after same set: equal again");

    a.set_all();
    b.set_all();
    check(a == b, "both full: equal");
});

// ── id_registry simulation ────────────────────────────────────────────────────
// End-to-end simulation of the id_registry's usage pattern.
// This is the most important integration test for the bitset.

LM_TEST_SUITE("bitset.id_registry_sim",
{
    lm::bitset<256> ids{};

    constexpr lm::st user_min  = 32;
    constexpr lm::st invalid   = 255;

    // Reserve framework IDs [0, 31]
    for(lm::st i = 0; i < user_min; ++i) ids.set(i);

    // get_fresh: first call should return 32
    auto fresh = ids.find_first_clear(user_min);
    check(fresh == 32,          "first get_fresh: 32");
    check(fresh < invalid,      "first get_fresh: not invalid");
    ids.set(fresh);

    // Second call: 33
    fresh = ids.find_first_clear(user_min);
    check(fresh == 33,          "second get_fresh: 33");
    ids.set(fresh);

    // Release 32, next fresh should be 32 again
    ids.clear(32);
    fresh = ids.find_first_clear(user_min);
    check(fresh == 32,          "after release(32): get_fresh == 32");
    ids.set(fresh);

    // Fill [32, 254] completely
    for(lm::st i = user_min; i < invalid; ++i) ids.set(i);

    // Exhausted — find_first_clear from user_min should find nothing < 255
    auto exhausted = ids.find_first_clear(user_min);
    check(exhausted == invalid || exhausted == lm::bitset<256>::npos,
          "exhausted: no valid id available");

    // count: 32 framework + 223 user = 255 taken, bit 255 free
    check(ids.count() == 255,   "exhausted: 255 bits set");
});
