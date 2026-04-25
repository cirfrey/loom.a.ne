#include "lm/test.hpp"
#include "lm/ini.hpp"

using namespace lm;

// ── number field ─────────────────────────────────────────────────────────────

LM_TEST_SUITE("ini.number.decimal",
{
    // Basic decimal
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("42"_text) == lm::ini::parse_result::ok, "decimal parse ok");
        check(out == 42u, "decimal value correct");
    }

    // Zero
    {
        lm::u32 out = 99;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("0"_text) == lm::ini::parse_result::ok, "zero parse ok");
        check(out == 0u, "zero value correct");
    }

    // Leading whitespace stripped
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("  99"_text) == lm::ini::parse_result::ok, "leading whitespace ok");
        check(out == 99u, "value after whitespace correct");
    }

    // Empty input rejected
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        auto r = f.parse(""_text);
        check(r != lm::ini::parse_result::ok, "empty input rejected");
    }
});

LM_TEST_SUITE("ini.number.hex",
{
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("0xFF"_text) == lm::ini::parse_result::ok, "0xFF parse ok");
        check(out == 255u, "0xFF value correct");
    }
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("0x00"_text) == lm::ini::parse_result::ok, "0x00 parse ok");
        check(out == 0u, "0x00 value correct");
    }
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("0xDEAD"_text) == lm::ini::parse_result::ok, "0xDEAD parse ok");
        check(out == 0xDEADu, "0xDEAD value correct");
    }
});

LM_TEST_SUITE("ini.number.bounds",
{
    // u8 overflow
    {
        lm::u8 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        auto r = f.parse("256"_text);
        check(r != lm::ini::parse_result::ok, "u8 overflow rejected");
        check_ctx(out == 0, "out unchanged on overflow", "out was mutated");
    }

    // Negative value into unsigned field
    {
        lm::u8 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        auto r = f.parse("-1"_text);
        check(r != lm::ini::parse_result::ok, "negative into u8 rejected");
    }

    // Max u8 is accepted
    {
        lm::u8 out = 0;
        auto f = lm::ini::number_field({"x"_text}, out, {});
        check(f.parse("255"_text) == lm::ini::parse_result::ok, "u8 max accepted");
        check(out == 255u, "u8 max value correct");
    }
});

LM_TEST_SUITE("ini.parse.section",
{
    {
        check(ini::parse(""_text, {}) == ini::parse_result::empty_input, "empty files are not okay");

    }
});

LM_TEST_SUITE("ini.parse.basic",
{
    {
        u32 out = 0;
        ini::field fields[] = {
            ini::number_field({"x"_text}, out, {})
        };

        auto r = ini::parse("x=42\n"_text, fields);
        check(r == ini::parse_result::ok, "basic parse ok");
        check(out == 42u, "value assigned");
    }
});

LM_TEST_SUITE("ini.parse.whitespace",
{
    {
        u32 out = 0;
        ini::field fields[] = {
            ini::number_field({"x"_text}, out, {})
        };

        auto r = ini::parse("   x   =   99   \n"_text, fields);
        check(r == ini::parse_result::ok, "whitespace parse ok");
        check(out == 99u, "whitespace trimmed correctly");
    }
});

LM_TEST_SUITE("ini.parse.comments",
{
    {
        u32 out = 0;
        ini::field fields[] = {
            ini::number_field({"x"_text}, out, {})
        };

        auto r = ini::parse(
            "; full line comment\n"
            "x=10 ; inline comment\n"_text,
            fields
        );

        check(r == ini::parse_result::ok, "comments ignored");
        check(out == 10u, "value parsed despite comment");
    }
});

LM_TEST_SUITE("ini.parse.sections",
{
    {
        u32 out = 0;
        ini::field fields[] = {
            ini::number_field({"core.value"_text}, out, {})
        };

        auto r = ini::parse(
            "[core]\n"
            "value=123\n"_text,
            fields
        );

        check(r == ini::parse_result::ok, "section parse ok");
        check(out == 123u, "section-prefixed key matched");
    }
});

LM_TEST_SUITE("ini.parse.unknown_keys",
{
    {
        u32 out = 5;
        ini::field fields[] = {
            ini::number_field({"x"_text}, out, {})
        };

        auto r = ini::parse("y=99\n"_text, fields, {.log_ignored=false});
        check(r == ini::parse_result::ok, "unknown key ignored");
        check(out == 5u, "known field untouched");
    }
});

LM_TEST_SUITE("ini.parse.duplicate_keys",
{
    {
        u32 out = 0;
        ini::field fields[] = {
            ini::number_field({"x"_text}, out, {})
        };

        auto r = ini::parse(
            "x=1\n"
            "x=2\n"_text,
            fields
        );

        check(r == ini::parse_result::ok, "duplicate keys ok");
        check(out == 2u, "last value wins");
    }
});

// TODO: test string truncation and test from mut_text.

LM_TEST_SUITE("ini.parse.quoted",
{
    {
        char buffer[32] = {};

        ini::field fields[] = {
            ini::string_field({"x"_text}, buffer, {.max_len = sizeof(buffer)})
        };

        auto r = ini::parse("x=\"hello world\"\n"_text, fields);
        check(r == ini::parse_result::ok, "quoted string parse ok");
        check(std::memcmp(buffer, "hello world", 11) == 0, "quoted value correct");
    }
});

LM_TEST_SUITE("ini.parse.parentheses",
{
    {
        char buffer[64] = {};

        ini::field fields[] = {
            ini::string_field({"x"_text}, buffer, {.max_len = sizeof(buffer)})
        };

        auto r = ini::parse("x=(abc(def)ghi)\n"_text, fields);

        check(r == ini::parse_result::ok, "parentheses parse ok");
        check(std::memcmp(buffer, "abc(def)ghi", 12) == 0, "nested parentheses handled");
    }
});
