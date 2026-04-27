#include "lm/ini.hpp"
#include "lm/log.hpp"
#include "lm/core/math.hpp"

#include <cstdlib>
#include <cinttypes>

static constexpr auto parse_string(
    lm::ini::field const& field,
    lm::text in,
    lm::ini::parse_args args,
    lm::text matched_key,
    lm::u8 key_idx
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
        if (field.string_data.too_large_behaviour == field.string_data.error) {
            if(args.log_parse_err)
                log::warn(
                    "[%.*s] string [%.*s] too big (%lu) for field (%lu) and truncation is off\n",
                    (int)matched_key.size, matched_key.data,
                    final_size, in.data + start,
                    final_size, field.string_data.max_len
                );
            return field_parse_result::string_too_big;
        }
        else if (field.string_data.too_large_behaviour == field.string_data.truncate) {
            // Leave room for the null terminator when truncating.
            st cap = field.string_data.max_len;
            if (field.string_data.add_null_terminator && cap > 0) cap -= 1;
            final_size = cap;
        }
    }

    auto* out = static_cast<char*>(field.get_output_for_idx(field.output, key_idx));
    if(!args.dry_run)
    {
        std::memcpy(out, in.data + start, final_size);
        if (field.string_data.size_out != nullptr)
        field.string_data.size_out(key_idx, final_size);
        if (field.string_data.add_null_terminator)
        out[final_size] = '\0';

        if (args.log_success) {
            log::debug(
                "[%.*s - string] = [%.*s]\n",
                (int)matched_key.size, matched_key.data,
                (int)final_size,       out
            );
        }
    }

    return field_parse_result::ok;
}

static constexpr auto parse_number(
    lm::ini::field const& field,
    lm::text in,
    lm::ini::parse_args args,
    lm::text matched_key,
    lm::u8 key_idx
) -> lm::ini::field_parse_result
{
    using namespace lm;
    using namespace lm::ini;

    // 1. Strip whitespace for number parsing
    st start = 0;
    st end = in.size;
    while (start < end && (in.data[start] == ' ' || in.data[start] == '\t')) ++start;
    while (end > start && (in.data[end - 1] == ' ' || in.data[end - 1] == '\t')) --end;

    if (start == end) {
        if(args.log_parse_err)
            log::warn(
                "[%.*s] was fed empty data\n",
                (int)matched_key.size, matched_key.data
            );
        return field_parse_result::empty_input;
    }

    auto true_start = start;

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
        auto pref = text{in.data + start, 2};
        if (pref == "0x" || pref == "0X") {
            base = 16;
            start += 2;
        } else if (pref == "0o" || pref == "0O") {
            base = 8;
            start += 2;
        } else if (pref == "0b" || pref == "0B") {
            base = 2;
            start += 2;
        }
    }
    if(
        (base == 2  && !field.number_data.allow_binary) ||
        (base == 8  && !field.number_data.allow_oct) ||
        (base == 10 && !field.number_data.allow_decimal) ||
        (base == 16  && !field.number_data.allow_hex)
    ) {
        if(args.log_parse_err)
            log::warn<200>(
                "[%.*s] number [%.*s] has unnalowed base [%u] (check field.number_data.allow_*)\n",
                (int)matched_key.size, matched_key.data,
                (int)(end - true_start), in.data + true_start,
                base
            );
        return field_parse_result::number_unallowed_base;
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
        else {
            if(args.log_parse_err)
                log::warn(
                    "[%.*s] unnalowed character within number [%.*s]\n",
                    (int)matched_key.size, matched_key.data,
                    (int)(end - true_start), in.data + true_start
                );
            return field_parse_result::number_unnalowed_character;
        }

        if (digit >= base) {
            if(args.log_parse_err)
                log::warn(
                    "[%.*s] unnalowed digit for base [%u] within number [%.*s]\n",
                    (int)matched_key.size, matched_key.data,
                    base,
                    (int)(end - true_start), in.data + true_start
                );
            return field_parse_result::number_unnalowed_digit;
        }

        // TODO: You could add overflow checks here for `val * base + digit`
        // if you want to be extra strictly safe against wrapping.
        val = val * base + digit;
    }

    #define LM_PARSE_NUMBER_LOG(FMT, OUT) \
        if (args.log_success) \
            log::debug(FMT, (int)matched_key.size, matched_key.data, OUT);

    // 5. Bounds Check and Cast
    u64 dry_output;
    void* output;
    if(args.dry_run) output = &dry_output;
    else             output = field.get_output_for_idx(field.output, key_idx);
    if (field.number_data.output_is_signed)
    {
        i64 sval = is_neg ? -static_cast<i64>(val) : static_cast<i64>(val);

        i64 type_min = 0;
        u64 type_max = 0;
        if (field.number_data.output_is_signed) {
            if (field.number_data.output_bits < 64) {
                i64 shift = field.number_data.output_bits - 1;
                type_min = -(i64{1} << shift);
                type_max = static_cast<u64>((1ULL << shift) - 1);
            } else {
                type_min = lm::signed_min<64>;
                type_max = static_cast<u64>(lm::signed_max<64>);
            }
        } else {
            type_min = 0;
            if (field.number_data.output_bits < 64) {
                type_max = (1ULL << field.number_data.output_bits) - 1;
            } else {
                type_max = lm::unsigned_max<64>;
            }
        }

        i64 effective_min = std::max(type_min, field.number_data.min);
        u64 effective_max = std::min(type_max, field.number_data.max);

        if (sval < effective_min || (sval >= 0 && static_cast<u64>(sval) > effective_max))
        {
            if (args.log_parse_err) {
                log::warn(
                    "[%.*s] number [%.*s] outside bounds [%" PRId64 "; %" PRIu64 "]\n",
                    (int)matched_key.size, matched_key.data,
                    (int)(end - true_start), in.data + true_start,
                    effective_min, effective_max
                );
            }
            return field_parse_result::number_outside_bounds;
        }

        switch (field.number_data.output_bits) {
            case 8:  *static_cast<i8* >(output) = static_cast<i8>(sval);  LM_PARSE_NUMBER_LOG("[%.*s - i8] = [%"  PRId8  "]\n", *static_cast<i8*>(output));  break;
            case 16: *static_cast<i16*>(output) = static_cast<i16>(sval); LM_PARSE_NUMBER_LOG("[%.*s - i16] = [%" PRId16 "]\n", *static_cast<i16*>(output)); break;
            case 32: *static_cast<i32*>(output) = static_cast<i32>(sval); LM_PARSE_NUMBER_LOG("[%.*s - i32] = [%" PRId32 "]\n", *static_cast<i32*>(output)); break;
            case 64: *static_cast<i64*>(output) = static_cast<i64>(sval); LM_PARSE_NUMBER_LOG("[%.*s - i64] = [%" PRId64 "]\n", *static_cast<i64*>(output)); break;
            default:
                if(args.log_bad_config)
                    log::warn(
                        "[%.*s] unknown bit size [%u] (allowed are 8, 16, 32, 64), can't write\n",
                        (int)matched_key.size, matched_key.data,
                        field.number_data.output_bits
                    );
                return field_parse_result::number_outside_bounds;
        }
    }
    else
    {
        if (is_neg && val != 0) {
            if(args.log_parse_err)
                log::warn(
                    "[%.*s] negative unsigned not allowed [%.*s]\n",
                    (int)matched_key.size, matched_key.data,
                    (int)(end - true_start), in.data + true_start
                );
            return field_parse_result::number_negative_unsigned;
        }
        u64 type_max = (field.number_data.output_bits < 64)
            ? (1ULL << field.number_data.output_bits) - 1
            : lm::unsigned_max<64>;
        u64 effective_max = std::min(type_max, field.number_data.max);
        u64 effective_min = (field.number_data.min > 0)
            ? static_cast<u64>(field.number_data.min)
            : 0ULL;

        if (val < effective_min || val > effective_max) {
            if (args.log_parse_err) {
                log::warn(
                    "[%.*s] number [%.*s] outside bounds [%" PRIu64 "; %" PRIu64 "]\n",
                    (int)matched_key.size, matched_key.data,
                    (int)(end - true_start), in.data + true_start,
                    effective_min, effective_max
                );
            }
            return field_parse_result::number_outside_bounds;
        }

        switch (field.number_data.output_bits) {
            case 8:  *static_cast<u8* >(output) = static_cast<u8>(val);  LM_PARSE_NUMBER_LOG("[%.*s - u8] = [%"  PRIu8  "]\n", *static_cast<u8*>(output));  break;
            case 16: *static_cast<u16*>(output) = static_cast<u16>(val); LM_PARSE_NUMBER_LOG("[%.*s - u16] = [%" PRIu16 "]\n", *static_cast<u16*>(output)); break;
            case 32: *static_cast<u32*>(output) = static_cast<u32>(val); LM_PARSE_NUMBER_LOG("[%.*s - u32] = [%" PRIu32 "]\n", *static_cast<u32*>(output)); break;
            case 64: *static_cast<u64*>(output) = static_cast<u64>(val); LM_PARSE_NUMBER_LOG("[%.*s - u64] = [%" PRIu64 "]\n", *static_cast<u64*>(output)); break;
            default:
                if(args.log_bad_config)
                    log::warn(
                        "[%.*s] unknown bit size [%u] (allowed are 8, 16, 32, 64), can't write\n",
                        (int)matched_key.size, matched_key.data,
                        field.number_data.output_bits
                    );
                return field_parse_result::number_unknown_size;
        }
    }

    return field_parse_result::ok;
}

static constexpr auto parse_enumeration(
    lm::ini::field const& field,
    lm::text in,
    lm::ini::parse_args args,
    lm::text matched_key,
    lm::u8 key_idx
) -> lm::ini::field_parse_result
{
    using field_parse_result = lm::ini::field_parse_result;

    if (field.enumeration_data.parse)
        return field.enumeration_data.parse(field, in, args, matched_key, key_idx);

    if (!args.log_bad_config)
        lm::log::warn(
            "[%.*s].enumeration_data.parse == nullptr\n",
            (int)matched_key.size, matched_key.data
        );
    return field_parse_result::enumeration_bad_config;
}

auto lm::ini::field::parse(text in, parse_args args, text matched_key, u8 key_idx) const -> field_parse_result
{
    if (matched_key.data == nullptr || matched_key.size == 0)
        matched_key = key;

    if (key.data == nullptr || key.size == 0 || keycount == 0) {
        if(args.log_bad_config)
            log::warn(
                "[%.*s] Fed empty input\n",
                (int)matched_key.size, matched_key.data
            );
        return field_parse_result::empty_key;
    }
    if (!output) {
        if(args.log_bad_config)
            log::warn(
                "[%.*s].output == nullptr\n",
                (int)matched_key.size, matched_key.data
            );
        return field_parse_result::empty_output;
    }

    switch (type)
    {
        case none:
            if(args.log_bad_config)
                log::warn(
                    "[%.*s].type == none\n",
                    (int)matched_key.size, matched_key.data
                );
            return field_parse_result::none_type;
        case string:      return parse_string(*this, in, args, matched_key, key_idx);
        case number:      return parse_number(*this, in, args, matched_key, key_idx);
        case enumeration: return parse_enumeration(*this, in, args, matched_key, key_idx);
        default:
            if(args.log_bad_config)
                log::warn(
                    "[%.*s].type == ???. NOTE: this shouldn't happen, likely an internal loomane error.\n",
                    (int)matched_key.size, matched_key.data
                );
            return field_parse_result::unknown_type;
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
    if(args.policy == parse_args::noop_on_any_error)
    {
        auto newargs = args;
        newargs.policy = parse_args::skip_on_error;
        newargs.dry_run = true;
        auto res = parse(input, fields, newargs, keys_to_parse, keys_to_skip);
        // Early return so we dont dry_run twice.
        if(args.dry_run == true) return res;
    }

    // Empty inputs are okay. As well as whitespace-only inputs, but that's handled later.
    if (!input.data || input.size == 0) { return parse_result::ok; }

    st   pos     = 0;
    text section = {};           // current section prefix — empty means no prefix
    parse_result last_error = parse_result::ok;

    // ── Cursor helpers ────────────────────────────────────────────────────────

    auto at_end  = [&]()         { return pos >= input.size; };
    auto cur     = [&]() -> char { return input.data[pos]; };
    auto advance = [&]()         { ++pos; };

    auto skip_line = [&]() {
        while (!at_end() && cur() != '\n') advance();
        if (!at_end()) advance(); // consume '\n'
    };

    auto skip_whitespace_inline = [&]() {
        while (!at_end() && (cur() == ' ' || cur() == '\t')) advance();
    };

    auto skip_whitespace = [&]() {
        while (!at_end() && (cur() == ' ' || cur() == '\t'
                          || cur() == '\r' || cur() == '\n')) advance();
    };

    auto strip = [](text t) -> text {
        st s = 0, e = t.size;
        while (s < e && (t.data[s]   == ' '  || t.data[s]   == '\t'
                      || t.data[s]   == '\r'  || t.data[s]   == '\n')) ++s;
        while (e > s && (t.data[e-1] == ' '  || t.data[e-1] == '\t'
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

    auto matches = [](text fieldkey, text sec, text key) -> bool
    {
        if (key.size == 0) return false;
        if (sec.size == 0) {
            if (fieldkey.size != key.size) return false;
            return __builtin_memcmp(fieldkey.data, key.data, key.size) == 0;
        }
        // fieldkey must be exactly: sec + '.' + key
        if (fieldkey.size != sec.size + 1 + key.size) return false;
        if (__builtin_memcmp(fieldkey.data, sec.data, sec.size) != 0) return false;
        if (fieldkey.data[sec.size] != '.') return false;
        return __builtin_memcmp(fieldkey.data + sec.size + 1, key.data, key.size) == 0;
    };

    // ── Main parse loop ───────────────────────────────────────────────────────

    while (!at_end())
    {
        skip_whitespace();
        if (at_end()) break;

        // ── Comment lines ─────────────────────────────────────────────────────
        if (cur() == ';' || cur() == '#') {
            skip_line();
            continue;
        }

        // ── Section header ────────────────────────────────────────────────────
        if (cur() == '[') {
            advance(); // consume '['
            st start = pos;
            while (!at_end() && cur() != ']' && cur() != '\n') advance();

            if (at_end() || cur() == '\n') {
                // Malformed — no closing ']'
                last_error = parse_result::sytax_error;
                if (args.log_parse_err)
                    lm::log::warn("[ini] malformed section header — missing ']'\n");
                if(args.policy == parse_args::stop_on_error) return last_error;
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
        while (!at_end() && cur() != '=' && cur() != '\n' && cur() != ';') advance();

        if (at_end() || cur() == '\n' || cur() == ';') {
            // TODO: finish this, it's not really working when [key ; comment]
            // No '=' — not a key=value line
            text raw = strip({input.data + key_start, pos - key_start});
            if (raw.size > 0 && args.log_parse_err)
                lm::log::debug("[ini] line without '=' ignored: [%.*s]\n",
                    (int)raw.size, raw.data);
            if (!at_end()) advance(); // consume '\n'
            continue;
        }

        text raw_key = strip({input.data + key_start, pos - key_start});
        advance(); // consume '='

        // Skip inline whitespace (spaces/tabs) after '=', not newlines.
        // Newlines belong to Form 1 parsing.
        skip_whitespace_inline();

        // ── Parse value ───────────────────────────────────────────────────────

        text value = {};

        if (!at_end() && cur() == '"')
        {
            // Form 2: "quoted string" — no escape sequences supported
            // If you need a quote character in the value, use Form 3: (value with "quotes")
            advance(); // consume opening '"'
            st val_start = pos;
            while (!at_end() && cur() != '"' && cur() != '\n') advance();

            // TODO: does this allow multiline strings?
            if (at_end() || cur() == '\n') {
                last_error = parse_result::sytax_error;
                if (args.log_parse_err)
                    lm::log::warn("[ini] unclosed '\"' for key [%.*s]\n",
                        (int)raw_key.size, raw_key.data);
                if(args.policy == parse_args::stop_on_error) return last_error;
                if (!at_end()) advance();
                continue; // skip this key-value pair
            }

            value = {input.data + val_start, pos - val_start};
            advance(); // consume closing '"'
            // Anything after the closing quote on the same line is ignored.
            skip_line();
        }
        else if (!at_end() && cur() == '(')
        {
            // Form 3: (parenthesised value) — nested parens are tracked
            // Comments inside are NOT stripped — the entire contents are the value.
            advance(); // consume opening '('
            st val_start = pos;
            st depth = 1;
            while (!at_end() && depth > 0) {
                if      (cur() == '(') ++depth;
                else if (cur() == ')') { if (--depth == 0) break; }
                advance();
            }

            // TODO: does this allow multiline strings?
            if (at_end() && depth > 0) {
                if (args.log_parse_err)
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
            while (!at_end() && cur() != '\n' && cur() != ';') advance();
            value = strip({input.data + val_start, pos - val_start});
            // If we stopped at ';', the rest of the line is a comment — skip it
            if (!at_end() && cur() == ';') skip_line();
        }

        // ── Dispatch to matching field ─────────────────────────────────────────

        if (raw_key.size == 0) continue; // blank key (= value with nothing before it)

        bool found   = false;
        bool matched = false;
        for (auto& f : fields) {
            if (matched && args.match_only_one_field) break;

            for (u8 key_idx = 0; key_idx < f.keycount; ++key_idx)  // BUG WAS: auto (deduced int)
            {
                char field_key_fmtbuf[config_t::ini_t::field_key_fmtbuf_size];
                auto field_key = f.get_key_for_idx(f.key, {field_key_fmtbuf, sizeof(field_key_fmtbuf)}, key_idx);

                if (!matches(field_key, section, raw_key)) continue;
                found = true;

                if (keys_to_parse.size())
                {
                    bool in_list = false;
                    for (auto k : keys_to_parse) {
                        if (matches(field_key, ""_text, k)) {
                            in_list = true;
                            break;
                        }
                    }
                    if (!in_list) continue;
                }

                if (keys_to_skip.size())
                {
                    bool in_list = false;
                    for (auto k : keys_to_skip) {
                        if (matches(field_key, ""_text, k)) {
                            in_list = true;
                            break;
                        }
                    }
                    if (in_list) continue;
                }

                auto r = f.parse(value, args, field_key, key_idx);
                if (r != field_parse_result::ok) {
                    last_error = parse_result::field_error;
                    if(args.policy == parse_args::stop_on_error) return last_error;
                    // Field-level parse errors log internally — we continue to allow
                    // duplicate keys later in the document to overwrite with valid values.
                }
                matched = true;
            }
        }

        if (!found && args.log_unknown_key) {
            if (section.size > 0)
                lm::log::warn("[ini] unknown key [%.*s.%.*s]\n",
                    (int)section.size, section.data,
                    (int)raw_key.size, raw_key.data);
            else
                lm::log::warn("[ini] unknown key [%.*s]\n",
                    (int)raw_key.size, raw_key.data);
        }
    }

    return last_error;
}
