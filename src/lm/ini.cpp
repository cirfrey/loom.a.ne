#include "lm/ini.hpp"
#include "lm/log.hpp"
#include "lm/core/math.hpp"

#include <cstdlib>
#include <cinttypes>

// TODO: implement error logging on all these functions.

static constexpr auto parse_string(
    lm::ini::field const& field,
    lm::text in,
    [[maybe_unused]] lm::ini::parse_args args
) -> lm::ini::field_parse_result
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
        if(field.string_data.too_large_behaviour == field.string_data.error) {
            return field_parse_result::string_too_big;
        }
        else if(field.string_data.too_large_behaviour == field.string_data.truncate)
            final_size = clamp(final_size, 0, field.string_data.max_len) - 1 * field.string_data.add_null_terminator;
    }

    auto* out = static_cast<char*>(field.output);
    std::memcpy(out, in.data + start, final_size);
    if(field.string_data.size_out != nullptr)
        *(st*)field.string_data.size_out = final_size;
    if(field.string_data.add_null_terminator)
        out[final_size + 1] = '\0';

    if(args.log_success) {
        log::debug(
            "[%.*s - string] = [%.*s]\n",
            (int)field.key.size, field.key.data,
            (int)final_size,     out
        );
    }

    return field_parse_result::ok;
}

static constexpr auto parse_number(
    lm::ini::field const& field,
    lm::text in,
    [[maybe_unused]] lm::ini::parse_args args
) -> lm::ini::field_parse_result
{
    using namespace lm;
    using namespace lm::ini;

    // 1. Strip whitespace for number parsing
    st start = 0;
    st end = in.size;
    while (start < end && (in.data[start] == ' ' || in.data[start] == '\t')) ++start;
    while (end > start && (in.data[end - 1] == ' ' || in.data[end - 1] == '\t')) --end;

    if (start == end) return field_parse_result::empty_input; // TODO: logme!

    // 2. Determine sign
    bool is_neg = false;
    if (in.data[start] == '-') {
        is_neg = true;
        ++start;
    } else if (in.data[start] == '+') {
        ++start;
    }

    // 3. Determine base (Hex, Bin, Dec)
    auto base = 10_u8;
    if (start + 2 <= end)
    {
        auto pref = text{in.data, 2};
        if (pref == "0x" || pref == "0X") {
            if(!field.number_data.allow_hex) return field_parse_result::number_unallowed_base; // TODO: logme!
            base = 16;
            start += 2;
        } else if (pref == "0o" || pref == "0O") {
            if(!field.number_data.allow_oct) return field_parse_result::number_unallowed_base; // TODO: logme!
            base = 8;
            start += 2;
        } else if (pref == "0b" || pref == "0B") {
            if(!field.number_data.allow_binary) return field_parse_result::number_unallowed_base; // TODO: logme!
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
        else return field_parse_result::number_unnalowed_character; // Invalid character

        if (digit >= base) return field_parse_result::number_unnalowed_digit; // E.g., '9' in binary or octal

        // TODO: You could add overflow checks here for `val * base + digit`
        // if you want to be extra strictly safe against wrapping.
        val = val * base + digit;
    }

    #define LM_PARSE_NUMBER_LOG(FMT, OUT) \
        if(args.log_success) \
            log::debug(FMT, (int)field.key.size, field.key.data, OUT);

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
            return field_parse_result::number_outside_bounds;

        switch (field.number_data.output_bits) {
            case 8:  *static_cast<i8* >(field.output) = static_cast<i8>(sval);  LM_PARSE_NUMBER_LOG("[%.*s - i8] = ["  PRId8  "]\n", *static_cast<i8*>(field.output));  break;
            case 16: *static_cast<i16*>(field.output) = static_cast<i16>(sval); LM_PARSE_NUMBER_LOG("[%.*s - i16] = [" PRId16 "]\n", *static_cast<i16*>(field.output)); break;
            case 32: *static_cast<i32*>(field.output) = static_cast<i32>(sval); LM_PARSE_NUMBER_LOG("[%.*s - i32] = [" PRId32 "]\n", *static_cast<i32*>(field.output)); break;
            case 64: *static_cast<i64*>(field.output) = static_cast<i64>(sval); LM_PARSE_NUMBER_LOG("[%.*s - i64] = [" PRId64 "]\n", *static_cast<i64*>(field.output)); break;
            default: return field_parse_result::number_outside_bounds;
        }
    }
    else
    {
        if (is_neg && val != 0) return field_parse_result::number_negative_unsigned;
        u64 type_max = (field.number_data.output_bits < 64)
            ? (u64{1} << field.number_data.output_bits) - 1
            : lm::unsigned_max<64>;
        u64 effective_max = type_max < field.number_data.max ? type_max : field.number_data.max;
        if (val > effective_max) return field_parse_result::number_outside_bounds;

        switch (field.number_data.output_bits) {
            case 8:  *static_cast<u8* >(field.output) = static_cast<u8>(val);  LM_PARSE_NUMBER_LOG("[%.*s - u8] = ["  PRIu8  "]\n", *static_cast<u8*>(field.output));  break;
            case 16: *static_cast<u16*>(field.output) = static_cast<u16>(val); LM_PARSE_NUMBER_LOG("[%.*s - u16] = [" PRIu16 "]\n", *static_cast<u16*>(field.output)); break;
            case 32: *static_cast<u32*>(field.output) = static_cast<u32>(val); LM_PARSE_NUMBER_LOG("[%.*s - u32] = [" PRIu32 "]\n", *static_cast<u32*>(field.output)); break;
            case 64: *static_cast<u64*>(field.output) = static_cast<u64>(val); LM_PARSE_NUMBER_LOG("[%.*s - u64] = [" PRIu64 "]\n", *static_cast<u64*>(field.output)); break;
            default: return field_parse_result::number_unknown_size;
        }
    }

    return field_parse_result::ok;
}

static constexpr auto parse_enumeration(
    lm::ini::field const& field,
    lm::text in,
    lm::ini::parse_args args
) -> lm::ini::field_parse_result
{
    using field_parse_result = lm::ini::field_parse_result;

    if (field.enumeration_data.parse)
        return field.enumeration_data.parse(field, in, args);

    if(!args.log_ignored) {
        lm::log::warn(
            "Skipped field: enum parse function pointer is null for [%.*s]\n",
            (int)field.key.size, field.key.data
        );
    }
    return field_parse_result::ok;
}

auto lm::ini::field::parse(text in, parse_args args) const -> field_parse_result
{
    if(key.data == nullptr || key.size == 0)
        return field_parse_result::empty_key;
    if(!output)
        return field_parse_result::empty_output;

    switch (type)
    {
        case none:        return field_parse_result::none_type;
        case string:      return parse_string(*this, in, args);
        case number:      return parse_number(*this, in, args);
        case enumeration: return parse_enumeration(*this, in, args);
        default:          return field_parse_result::unknown_type;
    }
}

auto lm::ini::parse(
    text input,
    std::span<field const> fields,
    parse_args args,
    std::span<text> keys_to_parse,
    std::span<text> keys_to_skip
) -> parse_result
{
    if(!input.data || input.size == 0) return parse_result::empty_input;

    st   pos     = 0;
    text section = {};           // current section prefix — empty means no prefix
    parse_result last_error = parse_result::ok;

    // ── Cursor helpers ────────────────────────────────────────────────────────

    auto at_end  = [&]()       { return pos >= input.size; };
    auto cur     = [&]() -> char { return input.data[pos]; };
    auto advance = [&]()       { ++pos; };

    auto skip_line = [&]() {
        while(!at_end() && cur() != '\n') advance();
        if(!at_end()) advance(); // consume '\n'
    };

    auto skip_whitespace_inline = [&]() {
        while(!at_end() && (cur() == ' ' || cur() == '\t')) advance();
    };

    auto skip_whitespace = [&]() {
        while(!at_end() && (cur() == ' ' || cur() == '\t'
                         || cur() == '\r' || cur() == '\n')) advance();
    };

    auto strip = [](text t) -> text {
        st s = 0, e = t.size;
        while(s < e && (t.data[s]   == ' '  || t.data[s]   == '\t'
                     || t.data[s]   == '\r'  || t.data[s]   == '\n')) ++s;
        while(e > s && (t.data[e-1] == ' '  || t.data[e-1] == '\t'
                     || t.data[e-1] == '\r'  || t.data[e-1] == '\n')) --e;
        return {t.data + s, e - s};
    };

    // ── Key matching — zero allocation, section-prefix aware ──────────────────
    //
    // field.key is always the full dotted path: "usb.midi.cable_count"
    // section is the active prefix: "usb.midi"
    // raw_key is the key from the current line: "cable_count"
    //
    // Match condition: field.key == section + "." + raw_key   (when section non-empty)
    //              or: field.key == raw_key                    (when section empty)

    auto matches = [](field const& f, text sec, text key) -> bool {
        if(key.size == 0) return false;
        if(sec.size == 0) {
            if(f.key.size != key.size) return false;
            return __builtin_memcmp(f.key.data, key.data, key.size) == 0;
        }
        // f.key must be exactly: sec + '.' + key
        if(f.key.size != sec.size + 1 + key.size) return false;
        if(__builtin_memcmp(f.key.data, sec.data, sec.size) != 0) return false;
        if(f.key.data[sec.size] != '.') return false;
        return __builtin_memcmp(f.key.data + sec.size + 1, key.data, key.size) == 0;
    };

    // ── Main parse loop ───────────────────────────────────────────────────────

    while(!at_end())
    {
        skip_whitespace();
        if(at_end()) break;

        // ── Comment lines ─────────────────────────────────────────────────────
        if(cur() == ';' || cur() == '#') {
            skip_line();
            continue;
        }

        // ── Section header ────────────────────────────────────────────────────
        if(cur() == '[') {
            advance(); // consume '['
            st start = pos;
            while(!at_end() && cur() != ']' && cur() != '\n') advance();

            if(at_end() || cur() == '\n') {
                // Malformed — no closing ']'
                if(args.log_ignored)
                    lm::log::warn("[ini] malformed section header — missing ']'\n");
                skip_line();
                continue;
            }

            section = strip({input.data + start, pos - start});
            advance(); // consume ']'
            skip_line(); // anything after ']' on the same line is ignored

            continue;
        }

        // ── Key = Value ───────────────────────────────────────────────────────

        // Parse key: everything up to '=' or '\n'
        st key_start = pos;
        while(!at_end() && cur() != '=' && cur() != '\n' && cur() != ';') advance();

        if(at_end() || cur() == '\n' || cur() == ';') {
            // TODO: finish this, it's not really working when [key ; comment]
            // No '=' — not a key=value line
            text raw = strip({input.data + key_start, pos - key_start});
            if(raw.size > 0 && args.log_ignored)
                lm::log::debug("[ini] line without '=' ignored: [%.*s]\n",
                    (int)raw.size, raw.data);
            if(!at_end()) advance(); // consume '\n'
            continue;
        }

        text raw_key = strip({input.data + key_start, pos - key_start});
        advance(); // consume '='

        // Skip inline whitespace (spaces/tabs) after '=', not newlines.
        // Newlines belong to Form 1 parsing.
        skip_whitespace_inline();

        // ── Parse value ───────────────────────────────────────────────────────

        text value = {};

        if(!at_end() && cur() == '"')
        {
            // Form 2: "quoted string" — no escape sequences supported
            // If you need a quote character in the value, use Form 3: (value with "quotes")
            advance(); // consume opening '"'
            st val_start = pos;
            while(!at_end() && cur() != '"' && cur() != '\n') advance();

            if(at_end() || cur() == '\n') {
                if(args.log_ignored)
                    lm::log::warn("[ini] unclosed '\"' for key [%.*s]\n",
                        (int)raw_key.size, raw_key.data);
                if(!at_end()) advance();
                continue; // skip this key-value pair
            }

            value = {input.data + val_start, pos - val_start};
            advance(); // consume closing '"'
            // Anything after the closing quote on the same line is ignored.
            skip_line();
        }
        else if(!at_end() && cur() == '(')
        {
            // Form 3: (parenthesised value) — nested parens are tracked
            // Comments inside are NOT stripped — the entire contents are the value.
            advance(); // consume opening '('
            st val_start = pos;
            st depth = 1;
            while(!at_end() && depth > 0) {
                if     (cur() == '(') ++depth;
                else if(cur() == ')') { if(--depth == 0) break; }
                advance();
            }

            if(at_end() && depth > 0) {
                if(args.log_ignored)
                    lm::log::warn("[ini] unclosed '(' for key [%.*s]\n",
                        (int)raw_key.size, raw_key.data);
                continue; // skip this key-value pair
            }

            value = {input.data + val_start, pos - val_start};
            advance(); // consume closing ')'
            skip_line(); // anything after ')' on the same line is ignored
        }
        else
        {
            // Form 1: raw value — until ';' (comment) or '\n', then stripped
            st val_start = pos;
            while(!at_end() && cur() != '\n' && cur() != ';') advance();
            value = strip({input.data + val_start, pos - val_start});
            // If we stopped at ';', the rest of the line is a comment — skip it
            if(!at_end() && cur() == ';') skip_line();
        }

        // ── Dispatch to matching field ─────────────────────────────────────────

        if(raw_key.size == 0) continue; // blank key (= value with nothing before it)

        bool found = false;
        for(auto& f : fields) {
            if(!matches(f, section, raw_key)) continue;
            found = true;

            if(keys_to_parse.size())
            {
                bool in_list = false;
                for(auto k : keys_to_parse) {
                    if(matches(f, ""_text, k)) {
                        in_list = true;
                        break;
                    }
                }
                if(!in_list) continue;
            }

            if(keys_to_skip.size())
            {
                bool in_list = false;
                for(auto k : keys_to_skip) {
                    if(matches(f, ""_text, k)) {
                        in_list = true;
                        break;
                    }
                }
                if(in_list) continue;
            }

            auto r = f.parse(value, args);
            if(r != parse_result::ok) {
                last_error = r;
                // Field-level parse errors log internally — we continue to allow
                // duplicate keys later in the document to overwrite with valid values.
            }

            if(args.match_only_one_field)
                break; // only dispatch to the first matching field
        }

        if(!found && args.log_ignored) {
            if(section.size > 0)
                lm::log::debug("[ini] unknown key [%.*s.%.*s]\n",
                    (int)section.size, section.data,
                    (int)raw_key.size, raw_key.data);
            else
                lm::log::debug("[ini] unknown key [%.*s]\n",
                    (int)raw_key.size, raw_key.data);
        }
    }

    return last_error;
}
