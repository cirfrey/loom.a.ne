#include "lm/test.hpp"
#include "lm/ini.hpp"

using namespace lm;

static constexpr auto loud = ini::parse_args{
    .log_success = true,
    .log_error   = true,
    .log_ignored = true,
};

// ── EXPECTED-PASS tests ───────────────────────────────────────────────────────
// All multikey fields use the new api:
//   ini::num("pattern_%u"_text, count, +[](u8 i) -> T* { return ...; })
//   ini::str("pattern_%u"_text, count, +[](u8 i) -> char* { return ...; }, data)
//   ini::feat("pattern_%u"_text, count, +[](u8 i) -> lm::feature* { return ...; })
//
// Getter lambdas must be non-capturing — test arrays are marked static and
// reset at the start of each suite.

LM_TEST_SUITE("ini.multikey.num.both_keys",
{
    static u32 out[2];
    out[0] = out[1] = 0;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("ch_0=10\nch_1=20\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "both keys: parse ok");
    check(out[0] == 10u, "both keys: slot 0 correct");
    check(out[1] == 20u, "both keys: slot 1 correct");
});

LM_TEST_SUITE("ini.multikey.num.partial_first_only",
{
    static u32 out[2];
    out[0] = 0; out[1] = 0xDEAD;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("ch_0=42\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "partial first: parse ok");
    check(out[0] == 42u,     "partial first: slot 0 written");
    check(out[1] == 0xDEADu, "partial first: slot 1 untouched");
});

LM_TEST_SUITE("ini.multikey.num.partial_second_only",
{
    static u32 out[2];
    out[0] = 0xDEAD; out[1] = 0;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("ch_1=99\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "partial second: parse ok");
    check(out[0] == 0xDEADu, "partial second: slot 0 untouched");
    check(out[1] == 99u,     "partial second: slot 1 written");
});

LM_TEST_SUITE("ini.multikey.num.reversed_order_in_file",
{
    // Dispatch is purely key-string based — file order must not matter.
    static u32 out[3];
    out[0] = out[1] = out[2] = 0;
    ini::field fields[] = {
        ini::num("x_%u"_text, 3u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("x_2=300\nx_0=100\nx_1=200\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "reversed order: parse ok");
    check(out[0] == 100u, "reversed order: slot 0 correct");
    check(out[1] == 200u, "reversed order: slot 1 correct");
    check(out[2] == 300u, "reversed order: slot 2 correct");
});

LM_TEST_SUITE("ini.multikey.num.duplicate_indexed_key_last_wins",
{
    static u32 out[2];
    out[0] = out[1] = 0;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("ch_0=1\nch_0=2\nch_1=10\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "duplicate indexed: parse ok");
    check(out[0] == 2u,  "duplicate indexed: last write wins");
    check(out[1] == 10u, "duplicate indexed: other slot unaffected");
});

LM_TEST_SUITE("ini.multikey.num.under_section",
{
    // Field key = "bus.ch_%u", section = "bus", raw_key = "ch_0" / "ch_1".
    static u32 out[2];
    out[0] = out[1] = 0;
    ini::field fields[] = {
        ini::num("bus.ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("[bus]\nch_0=11\nch_1=22\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "section multikey: parse ok");
    check(out[0] == 11u, "section multikey: slot 0");
    check(out[1] == 22u, "section multikey: slot 1");
});

LM_TEST_SUITE("ini.multikey.num.wrong_section_no_match",
{
    static u32 out[2];
    out[0] = out[1] = 0xBEEF;
    ini::field fields[] = {
        ini::num("bus.ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("[other]\nch_0=11\nch_1=22\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "wrong section: parse ok");
    check(out[0] == 0xBEEFu, "wrong section: slot 0 untouched");
    check(out[1] == 0xBEEFu, "wrong section: slot 1 untouched");
});

LM_TEST_SUITE("ini.multikey.num.u8_bounds_per_slot",
{
    static u8 out[2];
    out[0] = out[1] = 0xFF;
    ini::field fields[] = {
        ini::num("v_%u"_text, 2u, +[](u8 i) -> u8* { return &out[i]; })
    };
    // slot 0 valid (200), slot 1 overflows (256) — parse returns error
    auto r = ini::parse("v_0=200\nv_1=256\n"_text, fields, loud);
    check(r != ini::parse_result::ok, "u8 overflow on slot 1 detected");
    check(out[0] == 200u,  "slot 0 written correctly before error");
    check(out[1] == 0xFFu, "slot 1 not modified on overflow");
});

LM_TEST_SUITE("ini.multikey.str.basic",
{
    static constexpr st SLOT = 32;
    static char pool[3 * SLOT];
    std::memset(pool, 0, sizeof(pool));
    ini::field fields[] = {
        ini::str("name_%u"_text, 3u,
            +[](u8 i) -> char* { return pool + i * SLOT; },
            {.max_len = SLOT})
    };
    auto r = ini::parse("name_0=foo\nname_1=bar\nname_2=baz\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "str multikey: parse ok");
    check(std::memcmp(pool + 0 * SLOT, "foo", 3) == 0, "str slot 0 correct");
    check(std::memcmp(pool + 1 * SLOT, "bar", 3) == 0, "str slot 1 correct");
    check(std::memcmp(pool + 2 * SLOT, "baz", 3) == 0, "str slot 2 correct");
});

LM_TEST_SUITE("ini.multikey.str.partial_leaves_other_slots_clean",
{
    static constexpr st SLOT = 32;
    static char pool[2 * SLOT];
    std::memset(pool, 0xAB, sizeof(pool));
    ini::field fields[] = {
        ini::str("s_%u"_text, 2u,
            +[](u8 i) -> char* { return pool + i * SLOT; },
            {.max_len = SLOT})
    };
    auto r = ini::parse("s_0=hello\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "str partial: parse ok");
    check(std::memcmp(pool, "hello", 5) == 0, "str partial: slot 0 written");
    check(static_cast<unsigned char>(pool[SLOT]) == 0xABu, "str partial: slot 1 untouched");
});

LM_TEST_SUITE("ini.multikey.feat.basic",
{
    static lm::feature out[3];
    out[0] = out[1] = out[2] = lm::feature::off;
    ini::field fields[] = {
        ini::feat("f_%u"_text, 3u, +[](u8 i) -> lm::feature* { return &out[i]; })
    };
    auto r = ini::parse("f_0=yes\nf_1=disabled\nf_2=on\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "feat multikey: parse ok");
    check(out[0] == lm::feature::on,  "feat slot 0: yes → on");
    check(out[1] == lm::feature::off, "feat slot 1: disabled → off");
    check(out[2] == lm::feature::on,  "feat slot 2: on → on");
});

LM_TEST_SUITE("ini.multikey.feat.partial",
{
    static lm::feature out[2];
    out[0] = lm::feature::off;
    out[1] = lm::feature::on;  // sentinel — must not change
    ini::field fields[] = {
        ini::feat("f_%u"_text, 2u, +[](u8 i) -> lm::feature* { return &out[i]; })
    };
    auto r = ini::parse("f_0=enabled\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "feat partial: parse ok");
    check(out[0] == lm::feature::on, "feat partial: slot 0 written");
    check(out[1] == lm::feature::on, "feat partial: slot 1 sentinel intact");
});

LM_TEST_SUITE("ini.multikey.custom_formatter",
{
    // Use the multikey struct form to pass a custom formatter.
    // renum_formatter maps enum ordinals to unqualified names.
    // Here we use a trivially testable named-index enum.
    enum class ch : u8 { left, right };
    static u32 out[2];
    out[0] = out[1] = 0;
    ini::field fields[] = {
        ini::num<u32>(
            ini::multikey{
                .pattern   = "audio.%.*s"_text,
                .count     = 2,
                .formatter = ini::multikey::renum_formatter<ch>
            },
            +[](u8 i) -> u32* { return &out[i]; }
        )
    };
    // renum_formatter<ch> maps ordinal 0 → "left", 1 → "right"
    auto r = ini::parse("audio.left=100\naudio.right=200\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "custom formatter: parse ok");
    check(out[0] == 100u, "custom formatter: left slot correct");
    check(out[1] == 200u, "custom formatter: right slot correct");
});

LM_TEST_SUITE("ini.multikey.keys_to_parse_filters_specific_index",
{
    static u32 out[2];
    out[0] = out[1] = 0xAA;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    lm::text allowed[] = { "ch_1"_text };
    auto r = ini::parse("ch_0=10\nch_1=20\n"_text, fields, loud, allowed);
    check(r == ini::parse_result::ok, "keys_to_parse: parse ok");
    check(out[0] == 0xAAu, "keys_to_parse: ch_0 skipped");
    check(out[1] == 20u,   "keys_to_parse: ch_1 parsed");
});

LM_TEST_SUITE("ini.multikey.keys_to_skip_filters_specific_index",
{
    static u32 out[2];
    out[0] = out[1] = 0xBB;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    lm::text skipped[] = { "ch_0"_text };
    auto r = ini::parse("ch_0=10\nch_1=20\n"_text, fields, loud, {}, skipped);
    check(r == ini::parse_result::ok, "keys_to_skip: parse ok");
    check(out[0] == 0xBBu, "keys_to_skip: ch_0 skipped");
    check(out[1] == 20u,   "keys_to_skip: ch_1 parsed");
});

LM_TEST_SUITE("ini.multikey.match_only_one_field_does_not_cut_multikey_indices",
{
    // match_only_one_field only stops multiple *field descriptors* from claiming
    // the same line. It does not interrupt the key_idx loop within one descriptor.
    // ch_0 and ch_1 are different lines, so both must land.
    static u32 out_multi[2];
    out_multi[0] = out_multi[1] = 0;
    u32 out_plain = 0xBEEF;

    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out_multi[i]; }),
        ini::num("ch_0"_text, out_plain)   // would claim ch_0 if multikey didn't win
    };
    auto r = ini::parse("ch_0=1\nch_1=2\n"_text, fields,
                        {.log_success=false, .match_only_one_field=true});
    check(r == ini::parse_result::ok,      "match_only_one_field + multikey: parse ok");
    check(out_multi[0] == 1u,      "multikey slot 0 written");
    check(out_multi[1] == 2u,      "multikey slot 1 written");
    check(out_plain   == 0xBEEFu,  "plain duplicate field skipped");
});

// ── FOOTGUN tests — behavioral documentation ──────────────────────────────────
// These document surprising but intentional behavior.

LM_TEST_SUITE("ini.multikey.footgun.no_formatter_floods_all_slots",
{
    // no_formatter returns the pattern unchanged for every key_idx.
    // A single "x=7" in the ini matches ALL N slots because they all expand
    // to the same key string. With a pointer-arithmetic getter, every slot
    // gets written with the same value — silent data flood, no warning.
    static u32 out[3];
    out[0] = out[1] = out[2] = 0;
    ini::field fields[] = {
        ini::num<u32>(
            ini::multikey{
                .pattern   = "x"_text,
                .count     = 3,
                .formatter = ini::multikey::no_formatter
            },
            +[](u8 i) -> u32* { return &out[i]; }
        )
    };
    auto r = ini::parse("x=7\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "no formatter floods: parse succeeds");
    // One "x=7" writes to all three slots.
    check(out[0] == 7u, "slot 0 flooded");
    check(out[1] == 7u, "slot 1 flooded");
    check(out[2] == 7u, "slot 2 flooded");
    // The parser gives no warning. Mitigation: always pair a non-trivial getter
    // with a non-trivial formatter (u8_formatter or renum_formatter).
});

LM_TEST_SUITE("ini.multikey.footgun.out_of_range_index_silently_ignored",
{
    // A key that looks like slot 2 of a count=2 field ("ch_2") simply doesn't
    // match any slot and becomes an unknown key. parse returns ok with no error
    // and no log (log_ignored=false). A config file typo is completely invisible.
    static u32 out[2];
    out[0] = out[1] = 0xCC;
    ini::field fields[] = {
        ini::num("ch_%u"_text, 2u, +[](u8 i) -> u32* { return &out[i]; })
    };
    auto r = ini::parse("ch_2=99\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "out-of-range idx: ok, no error");
    check(out[0] == 0xCCu, "out-of-range idx: slot 0 untouched");
    check(out[1] == 0xCCu, "out-of-range idx: slot 1 untouched");
});

// ── REGRESSION tests — bugs that are now fixed ────────────────────────────────

LM_TEST_SUITE("ini.multikey.regression.enum_normalizer_called_at_idx0",
{
    // FIXED: default_enum_parser_for used to guard the redirector with
    // `if(key_idx >= 1)`, skipping it at idx=0. The normalizer would then
    // receive the getter function address instead of the actual data pointer.
    // Fix: always call get_output_for_idx regardless of key_idx.
    //
    // Verify: feat multikey at idx=0 must normalize correctly.
    static lm::feature out[2];
    out[0] = out[1] = lm::feature::off;
    ini::field fields[] = {
        ini::feat("f_%u"_text, 2u, +[](u8 i) -> lm::feature* { return &out[i]; })
    };
    auto r = ini::parse("f_0=yes\nf_1=no\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "normalizer at idx=0: parse ok");
    // If the bug were present, out[0] would hold garbage (the getter fn address
    // interpreted as feature). The fix ensures it holds lm::feature::on.
    check(out[0] == lm::feature::on,  "normalizer at idx=0: correctly on (FIXED)");
    check(out[1] == lm::feature::off, "normalizer at idx=1: correctly off");
});

LM_TEST_SUITE("ini.multikey.regression.str_null_terminator_at_final_size",
{
    // FIXED: parse_string wrote '\0' at out[final_size + 1] instead of
    // out[final_size], leaving out[final_size] uninitialised and writing one
    // byte past the string data — an out-of-bounds write when the buffer was
    // exactly final_size bytes.
    //
    // Verify: null is at buf[final_size], and buf[final_size+1] is untouched.
    static constexpr st SLOT = 32;
    static char pool[2 * SLOT];
    std::memset(pool, 0xAB, sizeof(pool));
    ini::field fields[] = {
        ini::str("s_%u"_text, 2u,
            +[](u8 i) -> char* { return pool + i * SLOT; },
            {.max_len = SLOT})
    };
    auto r = ini::parse("s_0=hello\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "null terminator fix: parse ok");
    // "hello" = 5 bytes → null at pool[5], pool[6] must still be poison.
    check(std::memcmp(pool, "hello", 5) == 0, "content correct");
    check(pool[5] == '\0',   "null at final_size (FIXED)");
    check(pool[6] == '\xAB', "one past null is poison — no OOB write (FIXED)");
    // Slot 1 start must be fully untouched.
    check(static_cast<unsigned char>(pool[SLOT]) == 0xABu, "slot 1 untouched");
});
