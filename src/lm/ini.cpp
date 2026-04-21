#include "lm/ini.hpp"
#include "lm/log.hpp"
#include "lm/core/math.hpp"

#include <cstdlib>

static constexpr auto parse_string(
    lm::ini::field& field,
    lm::text in,
    lm::ini::parse_args args
) -> lm::ini::parse_result
{
    using namespace lm;
    using namespace lm::ini;

    st start = 0;
    st end = in.size;

    if (field.string_data.strip)
    {
        auto is_space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
        while (start < end && is_space(in.data[start])) ++start;
        while (end > start && is_space(in.data[end - 1])) --end;
    }

    st final_size = end - start;
    if (final_size > field.string_data.max_len)
    {
        // String too big, log and ignore.
        return parse_result::string_too_big;
    }

    auto* out_text = static_cast<text*>(field.output);
    std::memcpy(field.output, in.data + start, final_size);

    return parse_result::ok;
}

static constexpr auto parse_number(
    lm::ini::field& field,
    lm::text in,
    lm::ini::parse_args args
) -> lm::ini::parse_result
{
    using namespace lm;
    using namespace lm::ini;

    // 1. Strip whitespace for number parsing
    st start = 0;
    st end = in.size;
    while (start < end && (in.data[start] == ' ' || in.data[start] == '\t')) ++start;
    while (end > start && (in.data[end - 1] == ' ' || in.data[end - 1] == '\t')) --end;

    if (start == end) return parse_result::empty_input; // TODO: logme!

    // 2. Determine sign
    bool is_neg = false;
    if (in.data[start] == '-') {
        is_neg = true;
        ++start;
    } else if (in.data[start] == '+') {
        ++start;
    }

    // 3. Determine base (Hex, Bin, Dec)
    int base = 10;
    if (start + 2 <= end)
    {
        auto pref = text{in.data, 2};
        if (pref == "0x" || pref == "0X") {
            if(!field.number_data.allow_hex) return parse_result::number_unallowed_base; // TODO: logme!
            base = 16;
            start += 2;
        } else if (pref == "0o" || pref == "0O") {
            if(!field.number_data.allow_oct) return parse_result::number_unallowed_base; // TODO: logme!
            base = 8;
            start += 2;
        } else if (pref == "0b" || pref == "0B") {
            if(!field.number_data.allow_binary) return parse_result::number_unallowed_base; // TODO: logme!
            base = 2;
            start += 2;
        }
    }

    // 4. Parse the digits
    u64 val = 0;
    for (st i = start; i < end; ++i)
    {
        char c = in.data[i];
        u64 digit = 0;

        if      (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10; // For hex.
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10; // For hex.
        else if (c == '_' || c == '\'') continue; // Allowed separators.
        else return parse_result::number_unnalowed_character; // Invalid character

        if (digit >= base) return parse_result::number_unnalowed_digit; // E.g., '9' in binary or octal

        // TODO: You could add overflow checks here for `val * base + digit`
        // if you want to be extra strictly safe against wrapping.
        val = val * base + digit;
    }

    // 5. Bounds Check and Cast
    if (field.number_data.output_is_signed)
    {
        i64 sval = is_neg ? -static_cast<i64>(val) : static_cast<i64>(val);

        i64 type_max_s = (field.number_data.output_bits < 64)
            ? (i64{1} << (field.number_data.output_bits - 1)) - 1
            : lm::signed_max<64>;
        i64 type_min_s = (field.number_data.output_bits < 64)
            ? -(i64{1} << (field.number_data.output_bits - 1))
            : lm::signed_min<64>;
        i64 effective_max = type_max_s < static_cast<i64>(field.number_data.max)
                        ? type_max_s : static_cast<i64>(field.number_data.max);
        i64 effective_min = type_min_s > field.number_data.min
                        ? type_min_s : field.number_data.min;
        if (sval < effective_min || sval > effective_max)
            return parse_result::number_outside_bounds;

        switch (field.number_data.output_bits) {
            case 8:  *static_cast<i8* >(field.output) = static_cast<i8>(sval);  break;
            case 16: *static_cast<i16*>(field.output) = static_cast<i16>(sval); break;
            case 32: *static_cast<i32*>(field.output) = static_cast<i32>(sval); break;
            case 64: *static_cast<i64*>(field.output) = static_cast<i64>(sval); break;
            default: return parse_result::number_outside_bounds;
        }
    }
    else
    {
        if (is_neg && val != 0) return parse_result::number_negative_unsigned;
        u64 type_max = (field.number_data.output_bits < 64)
            ? (u64{1} << field.number_data.output_bits) - 1
            : lm::unsigned_max<64>;
        u64 effective_max = type_max < field.number_data.max ? type_max : field.number_data.max;
        if (val > effective_max) return parse_result::number_outside_bounds;

        switch (field.number_data.output_bits) {
            case 8:  *static_cast<u8* >(field.output) = static_cast<u8>(val);  break;
            case 16: *static_cast<u16*>(field.output) = static_cast<u16>(val); break;
            case 32: *static_cast<u32*>(field.output) = static_cast<u32>(val); break;
            case 64: *static_cast<u64*>(field.output) = static_cast<u64>(val); break;
            default: return parse_result::number_unknown_size;
        }
    }
    return parse_result::ok;
}

static constexpr auto parse_enumeration(
    lm::ini::field& field,
    lm::text in,
    lm::ini::parse_args args
) -> lm::ini::parse_result
{
    using parse_result = lm::ini::parse_result;

    if (field.enumeration_data.parse)
        return field.enumeration_data.parse(field, in, args);

    if(!args.log_ignored) {
        lm::log::warn(
            "Skipped field: enum parse function pointer is null for [%.*s]\n",
            (int)field.key.size, field.key.data
        );
    }
    return parse_result::ok;
}

auto lm::ini::field::parse(text in, parse_args args) -> parse_result
{
    if(key.data == nullptr || key.size == 0)
        return parse_result::empty_key;
    if(!output)
        return parse_result::empty_output;

    switch (type)
    {
        case none:        return parse_result::none_type;
        case string:      return parse_string(*this, in, args);
        case number:      return parse_number(*this, in, args);
        case enumeration: return parse_enumeration(*this, in, args);
        default:          return parse_result::unknown_type;
    }
}
