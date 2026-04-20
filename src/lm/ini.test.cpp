#include "lm/test.hpp"
#include "lm/ini.hpp"

using namespace lm;

// ── number field ─────────────────────────────────────────────────────────────

LM_TEST_SUITE("ini.number.decimal",
{
    // Basic decimal
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("42"_text) == lm::ini::parse_result::ok, "decimal parse ok");
        check(out == 42u, "decimal value correct");
    }

    // Zero
    {
        lm::u32 out = 99;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("0"_text) == lm::ini::parse_result::ok, "zero parse ok");
        check(out == 0u, "zero value correct");
    }

    // Leading whitespace stripped
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("  99"_text) == lm::ini::parse_result::ok, "leading whitespace ok");
        check(out == 99u, "value after whitespace correct");
    }

    // Empty input rejected
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        auto r = f.parse(""_text);
        check(r != lm::ini::parse_result::ok, "empty input rejected");
    }
});

LM_TEST_SUITE("ini.number.hex",
{
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("0xFF"_text) == lm::ini::parse_result::ok, "0xFF parse ok");
        check(out == 255u, "0xFF value correct");
    }
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("0x00"_text) == lm::ini::parse_result::ok, "0x00 parse ok");
        check(out == 0u, "0x00 value correct");
    }
    {
        lm::u32 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("0xDEAD"_text) == lm::ini::parse_result::ok, "0xDEAD parse ok");
        check(out == 0xDEADu, "0xDEAD value correct");
    }
});

LM_TEST_SUITE("ini.number.bounds",
{
    // u8 overflow
    {
        lm::u8 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        auto r = f.parse("256"_text);
        check(r != lm::ini::parse_result::ok, "u8 overflow rejected");
        check_ctx(out == 0, "out unchanged on overflow", "out was mutated");
    }

    // Negative value into unsigned field
    {
        lm::u8 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        auto r = f.parse("-1"_text);
        check(r != lm::ini::parse_result::ok, "negative into u8 rejected");
    }

    // Max u8 is accepted
    {
        lm::u8 out = 0;
        auto f = lm::ini::number_field("x"_text, out, {});
        check(f.parse("255"_text) == lm::ini::parse_result::ok, "u8 max accepted");
        check(out == 255u, "u8 max value correct");
    }
});
