#include "lm/test.hpp"
#include "lm/ini.hpp"

using namespace lm;

static constexpr auto loud = ini::parse_args{
    .log_success = true,
    .log_error   = true,
    .log_ignored = true,
};

// ── num field — direct parse ──────────────────────────────────────────────────

LM_TEST_SUITE("ini.num.decimal",
{
    { u32 out = 0;  auto f = ini::num("x"_text, out); check(f.parse("42"_text,  loud) == ini::parse_result::ok, "parse ok");    check(out == 42u,  "value correct"); }
    { u32 out = 99; auto f = ini::num("x"_text, out); check(f.parse("0"_text,   loud) == ini::parse_result::ok, "zero ok");     check(out == 0u,   "zero correct"); }
    { u32 out = 0;  auto f = ini::num("x"_text, out); check(f.parse("  99"_text,loud) == ini::parse_result::ok, "leading ws");  check(out == 99u,  "ws stripped"); }
    { u32 out = 0;  auto f = ini::num("x"_text, out); check(f.parse(""_text,    loud) != ini::parse_result::ok, "empty input rejected"); }
});

LM_TEST_SUITE("ini.num.hex",
{
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("0xFF"_text,   loud) == ini::parse_result::ok, "0xFF ok");   check(out == 255u,    "0xFF"); }
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("0x00"_text,   loud) == ini::parse_result::ok, "0x00 ok");   check(out == 0u,      "0x00"); }
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("0xDEAD"_text, loud) == ini::parse_result::ok, "0xDEAD ok"); check(out == 0xDEADu, "0xDEAD"); }
});

LM_TEST_SUITE("ini.num.binary",
{
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("0b1010"_text, loud) == ini::parse_result::ok, "binary ok"); check(out == 10u, "0b1010 == 10"); }
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("0b0"_text,    loud) == ini::parse_result::ok, "0b0 ok");    check(out == 0u,  "0b0 == 0"); }
});

LM_TEST_SUITE("ini.num.separators",
{
    // '_' and '\'' are allowed digit separators
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("1_000_000"_text, loud) == ini::parse_result::ok, "underscores"); check(out == 1000000u, "1_000_000"); }
    { u32 out = 0; auto f = ini::num("x"_text, out); check(f.parse("1'000"_text,     loud) == ini::parse_result::ok, "tick sep");   check(out == 1000u,    "1'000"); }
});

LM_TEST_SUITE("ini.num.bounds.u8",
{
    // u8 overflow rejected, output unchanged
    {
        u8 out = 0;
        auto f = ini::num("x"_text, out);
        auto r = f.parse("256"_text, loud);
        check(r != ini::parse_result::ok, "u8 256 rejected");
        check(out == 0u, "out unchanged on overflow");
    }
    // Negative into unsigned rejected
    {
        u8 out = 0;
        auto f = ini::num("x"_text, out);
        check(f.parse("-1"_text, loud) != ini::parse_result::ok, "negative u8 rejected");
    }
    // Exactly u8 max accepted
    {
        u8 out = 0;
        auto f = ini::num("x"_text, out);
        check(f.parse("255"_text, loud) == ini::parse_result::ok, "u8 max ok");
        check(out == 255u, "u8 max correct");
    }
});

LM_TEST_SUITE("ini.num.bounds.i8",
{
    { i8 out = 0; auto f = ini::num("x"_text, out); check(f.parse("-128"_text, loud) == ini::parse_result::ok, "i8 min ok");  check(out == -128, "i8 min"); }
    { i8 out = 0; auto f = ini::num("x"_text, out); check(f.parse("127"_text,  loud) == ini::parse_result::ok, "i8 max ok");  check(out == 127,  "i8 max"); }
    { i8 out = 0; auto f = ini::num("x"_text, out); check(f.parse("128"_text,  loud) != ini::parse_result::ok, "i8+1 rejected"); }
    { i8 out = 0; auto f = ini::num("x"_text, out); check(f.parse("-129"_text, loud) != ini::parse_result::ok, "i8-1 rejected"); }
});

LM_TEST_SUITE("ini.num.bounds.custom_max",
{
    // field-level max constraint tighter than type max
    {
        u32 out = 0;
        auto f = ini::num("x"_text, out, {.max = 10});
        check(f.parse("10"_text, loud) == ini::parse_result::ok, "at max ok");
        check(out == 10u, "at max value");
    }
    {
        u32 out = 99;
        auto f = ini::num("x"_text, out, {.max = 10});
        check(f.parse("11"_text, loud) != ini::parse_result::ok, "above max rejected");
        check(out == 99u, "out unchanged");
    }
});

LM_TEST_SUITE("ini.num.disallowed_base",
{
    { u32 out = 0; auto f = ini::num("x"_text, out, {.allow_hex=false});    check(f.parse("0xFF"_text,   loud) != ini::parse_result::ok, "hex disallowed"); }
    { u32 out = 0; auto f = ini::num("x"_text, out, {.allow_binary=false}); check(f.parse("0b1010"_text, loud) != ini::parse_result::ok, "bin disallowed"); }
    { u32 out = 0; auto f = ini::num("x"_text, out, {.allow_oct=false});    check(f.parse("0o77"_text,   loud) != ini::parse_result::ok, "oct disallowed"); }
});

// ── str field — direct parse ──────────────────────────────────────────────────

LM_TEST_SUITE("ini.str.basic",
{
    char buf[32] = {};
    auto f = ini::str("s"_text, buf, {.max_len = sizeof(buf)});
    check(f.parse("hello"_text, loud) == ini::parse_result::ok, "str parse ok");
    check(std::memcmp(buf, "hello", 5) == 0, "content correct");
    check(buf[5] == '\0', "null terminated");
});

LM_TEST_SUITE("ini.str.strip",
{
    // Stripping enabled by default for str builder
    char buf[32] = {};
    auto f = ini::str("s"_text, buf, {.max_len = sizeof(buf), .strip = true});
    check(f.parse("  hello  "_text, loud) == ini::parse_result::ok, "strip parse ok");
    check(std::memcmp(buf, "hello", 5) == 0, "whitespace stripped");
    check(buf[5] == '\0', "null terminated after strip");
});

LM_TEST_SUITE("ini.str.too_large_error",
{
    char buf[4] = {};
    auto f = ini::str("s"_text, buf, {.max_len = sizeof(buf), .too_large_behaviour = ini::field::string_data_t::error});
    check(f.parse("toolong"_text, loud) == ini::parse_result::string_too_big, "error on overflow");
});

LM_TEST_SUITE("ini.str.too_large_truncate",
{
    char buf[4] = {};
    std::memset(buf, 0xAB, sizeof(buf));
    // max_len=4, add_null_terminator=true → effective capacity = 3 chars + null
    auto f = ini::str("s"_text, buf, {.max_len = sizeof(buf), .too_large_behaviour = ini::field::string_data_t::truncate});
    check(f.parse("toolong"_text, loud) == ini::parse_result::ok, "truncate ok");
    check(buf[3] == '\0', "null at capacity boundary");
});

LM_TEST_SUITE("ini.str.null_terminator_off_by_one_fix",
{
    // Regression for the fixed off-by-one: out[final_size+1] was '\0', leaving
    // out[final_size] uninitialised. After fix: out[final_size] is '\0'.
    static char buf[8];
    std::memset(buf, 0xAB, sizeof(buf));
    ini::field fields[] = { ini::str("s"_text, buf, {.max_len = sizeof(buf)}) };
    auto r = ini::parse("s=hello\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "null terminator fix: parse ok");
    check(std::memcmp(buf, "hello", 5) == 0, "content correct");
    check(buf[5] == '\0',   "buf[final_size] is null");
    check(buf[6] == '\xAB', "buf[final_size+1] is poison — no OOB write");
});

LM_TEST_SUITE("ini.str.mut_text_variant",
{
    // mut_text overload infers max_len from the view
    char buf[32] = {};
    auto f = ini::str("s"_text, mut_text{buf, sizeof(buf)});
    check(f.parse("world"_text, loud) == ini::parse_result::ok, "mut_text parse ok");
    check(std::memcmp(buf, "world", 5) == 0, "content correct");
});

// ── enm field ─────────────────────────────────────────────────────────────────

LM_TEST_SUITE("ini.enm.basic",
{
    enum class color : u8 { red, green, blue };
    color out = color::red;
    auto f = ini::enm("c"_text, out);
    check(f.parse("green"_text, loud) == ini::parse_result::ok, "enm parse ok");
    check(out == color::green, "enum value correct");
});

LM_TEST_SUITE("ini.enm.not_found",
{
    enum class color : u8 { red, green, blue };
    color out = color::red;
    auto f = ini::enm("c"_text, out);
    check(f.parse("purple"_text, loud) == ini::parse_result::enumeration_not_found, "unknown enum rejected");
    check(out == color::red, "out unchanged on not-found");
});

// ── feat field (normalized enumeration) ──────────────────────────────────────

LM_TEST_SUITE("ini.feat.all_vocabulary",
{
    // All six ini vocabulary words must normalize to the correct lm::feature.
    struct { text word; lm::feature expected; } cases[] = {
        { "off"_text,      lm::feature::off },
        { "disabled"_text, lm::feature::off },
        { "no"_text,       lm::feature::off },
        { "on"_text,       lm::feature::on  },
        { "enabled"_text,  lm::feature::on  },
        { "yes"_text,      lm::feature::on  },
    };
    for (auto& [word, expected] : cases)
    {
        lm::feature out = lm::feature::off;
        auto f = ini::feat("x"_text, out);
        auto r = f.parse(word, loud);
        check(r == ini::parse_result::ok, "feat parse ok");
        check(out == expected, "feat normalized correctly");
    }
});

LM_TEST_SUITE("ini.feat.not_found",
{
    lm::feature out = lm::feature::off;
    auto f = ini::feat("x"_text, out);
    check(f.parse("maybe"_text, loud) == ini::parse_result::enumeration_not_found, "bad feat rejected");
    check(out == lm::feature::off, "out unchanged");
});

// ── parse — file-level ────────────────────────────────────────────────────────

LM_TEST_SUITE("ini.parse.empty_input",
{
    check(ini::parse(""_text, {}, loud) == ini::parse_result::empty_input, "empty string is empty_input");
});

LM_TEST_SUITE("ini.parse.basic",
{
    u32 out = 0;
    ini::field fields[] = { ini::num("x"_text, out) };
    check(ini::parse("x=42\n"_text, fields, loud) == ini::parse_result::ok, "parse ok");
    check(out == 42u, "value assigned");
});

LM_TEST_SUITE("ini.parse.whitespace_around_key_and_value",
{
    u32 out = 0;
    ini::field fields[] = { ini::num("x"_text, out) };
    check(ini::parse("   x   =   99   \n"_text, fields, loud) == ini::parse_result::ok, "whitespace ok");
    check(out == 99u, "value trimmed correctly");
});

LM_TEST_SUITE("ini.parse.comment_semicolon",
{
    u32 out = 0;
    ini::field fields[] = { ini::num("x"_text, out) };
    auto r = ini::parse(
        "; full line comment\n"
        "x=10 ; inline comment\n"_text,
        fields, loud
    );
    check(r == ini::parse_result::ok, "comments ignored");
    check(out == 10u, "value parsed despite comment");
});

LM_TEST_SUITE("ini.parse.comment_hash",
{
    u32 out = 0;
    ini::field fields[] = { ini::num("x"_text, out) };
    auto r = ini::parse(
        "# hash comment\n"
        "x=7\n"_text,
        fields, loud
    );
    check(r == ini::parse_result::ok, "hash comment ignored");
    check(out == 7u, "value parsed");
});

LM_TEST_SUITE("ini.parse.section",
{
    u32 out = 0;
    ini::field fields[] = { ini::num("core.value"_text, out) };
    auto r = ini::parse(
        "[core]\n"
        "value=123\n"_text,
        fields, loud
    );
    check(r == ini::parse_result::ok, "section parse ok");
    check(out == 123u, "section-prefixed key matched");
});

LM_TEST_SUITE("ini.parse.section_reset",
{
    // A later section must not bleed into keys from a prior section
    u32 a = 0, b = 0;
    ini::field fields[] = {
        ini::num("s1.val"_text, a),
        ini::num("s2.val"_text, b),
    };
    auto r = ini::parse(
        "[s1]\nval=1\n"
        "[s2]\nval=2\n"_text,
        fields, loud
    );
    check(r == ini::parse_result::ok, "two sections ok");
    check(a == 1u, "s1.val correct");
    check(b == 2u, "s2.val correct");
});

LM_TEST_SUITE("ini.parse.unknown_keys_ignored",
{
    u32 out = 5;
    ini::field fields[] = { ini::num("x"_text, out) };
    auto r = ini::parse("y=99\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "unknown key ignored");
    check(out == 5u, "known field untouched");
});

LM_TEST_SUITE("ini.parse.duplicate_keys_last_wins",
{
    u32 out = 0;
    ini::field fields[] = { ini::num("x"_text, out) };
    auto r = ini::parse("x=1\nx=2\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "duplicate keys ok");
    check(out == 2u, "last value wins");
});

LM_TEST_SUITE("ini.parse.quoted_string",
{
    char buf[32] = {};
    ini::field fields[] = { ini::str("x"_text, buf, {.max_len = sizeof(buf)}) };
    auto r = ini::parse("x=\"hello world\"\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "quoted string parse ok");
    check(std::memcmp(buf, "hello world", 11) == 0, "quoted value correct");
});

LM_TEST_SUITE("ini.parse.parentheses_value",
{
    char buf[64] = {};
    ini::field fields[] = { ini::str("x"_text, buf, {.max_len = sizeof(buf)}) };
    auto r = ini::parse("x=(abc(def)ghi)\n"_text, fields, loud);
    check(r == ini::parse_result::ok, "paren parse ok");
    // content is "abc(def)ghi" — 11 chars + null
    check(std::memcmp(buf, "abc(def)ghi", 12) == 0, "nested parens handled");
});

LM_TEST_SUITE("ini.parse.match_only_one_field",
{
    // Two fields claiming the same key — only the first should win
    u32 first = 0, second = 0xBEEF;
    ini::field fields[] = {
        ini::num("x"_text, first),
        ini::num("x"_text, second),
    };
    auto r = ini::parse("x=7\n"_text, fields, {.log_success=false, .match_only_one_field=true});
    check(r == ini::parse_result::ok, "match_only_one_field: parse ok");
    check(first  == 7u,      "match_only_one_field: first field written");
    check(second == 0xBEEFu, "match_only_one_field: second field skipped");
});

LM_TEST_SUITE("ini.parse.keys_to_parse_filter",
{
    u32 a = 0xAA, b = 0xBB;
    ini::field fields[] = { ini::num("a"_text, a), ini::num("b"_text, b) };
    lm::text allowed[] = { "b"_text };
    auto r = ini::parse("a=1\nb=2\n"_text, fields, loud, allowed);
    check(r == ini::parse_result::ok, "keys_to_parse: parse ok");
    check(a == 0xAAu, "keys_to_parse: a skipped");
    check(b == 2u,    "keys_to_parse: b parsed");
});

LM_TEST_SUITE("ini.parse.keys_to_skip_filter",
{
    u32 a = 0xAA, b = 0xBB;
    ini::field fields[] = { ini::num("a"_text, a), ini::num("b"_text, b) };
    lm::text skipped[] = { "a"_text };
    auto r = ini::parse("a=1\nb=2\n"_text, fields, loud, {}, skipped);
    check(r == ini::parse_result::ok, "keys_to_skip: parse ok");
    check(a == 0xAAu, "keys_to_skip: a skipped");
    check(b == 2u,    "keys_to_skip: b parsed");
});

LM_TEST_SUITE("ini.parse.feat_via_file",
{
    lm::feature out = lm::feature::off;
    ini::field fields[] = { ini::feat("toggle"_text, out) };
    check(ini::parse("toggle=yes\n"_text, fields, loud) == ini::parse_result::ok, "feat via file: ok");
    check(out == lm::feature::on, "feat via file: normalized");
});

LM_TEST_SUITE("ini.parse.enm_via_file",
{
    enum class mode : u8 { fast, slow, auto_ };
    mode out = mode::fast;
    ini::field fields[] = { ini::enm("mode"_text, out) };
    check(ini::parse("mode=slow\n"_text, fields, loud) == ini::parse_result::ok, "enm via file: ok");
    check(out == mode::slow, "enm via file: correct");
});
