#pragma once

#include "lm/core.hpp"
#include "lm/log.hpp"

namespace lm::ini
{
    // The ini-facing version of lm::feature, use this for parsing and normalize into lm::feature.
    // If we don't do this the end-user can only use [off] and [on], and the end-user is king so...
    // NOTE: If modifying, keep the [0, 1] in the first two fields,
    //       otherwise end-user will get nonsense output during parsing.
    // NOTE: it's very important both of the enums have the same size and alignment.
    enum class feature : u8
    {
        disabled, enabled,
        no, yes,
        off, on,
    };

    constexpr auto to_feature(feature f) -> lm::feature
    {
        if(
            f == feature::off ||
            f == feature::disabled ||
            f == feature::no
        ) return lm::feature::off;
        return lm::feature::on;
    }

    enum class field_parse_result
    {
        ok,

        empty_key,
        empty_input,
        empty_output,
        none_type,
        unknown_type,

        number_unallowed_base,      // E.g: 0b when number_data.allow_binary == false.
        number_unnalowed_character, // E.g: 892abc.
        number_unnalowed_digit,     // E.g: 0b1155801.
        number_outside_bounds,
        number_unknown_size,
        number_negative_unsigned,

        string_too_big,

        enumeration_not_found,
        enumeration_bad_config,
    };

    struct parse_args
    {
        bool log_success = true;
        bool log_parse_err = true;
        bool log_bad_config = true;
        bool log_unknown_key = true;

        bool dry_run = false;

        enum parse_policy {
            noop_on_any_error, // If anything errors at all, nothing should be written.
            skip_on_error,
            stop_on_error,
        } policy = skip_on_error;

        bool match_only_one_field = false;
    };

    struct field
    {
        [[nodiscard]] auto parse(text, parse_args = parse_args{}, text matched_key = {}, u8 key_idx = 1) const -> field_parse_result;

        // output semantics depend on get_output_for_idx:
        //   no_redirector:        output is a raw Num* / char* / Enum* destination
        //   default_redirector<T>: output is a getter_t<T> fn ptr cast to void*
        text  key    = {nullptr, 0};
        void* output = nullptr;

        enum type_t {
            none,
            number,
            string,
            enumeration,
        } type = none;

        u8 keycount = 1;
        using get_key_for_idx_t    = text (*)(text key, mut_text buf, u8 key_idx);
        using get_output_for_idx_t = void*(*)(void* output, u8 key_idx);
        get_key_for_idx_t    get_key_for_idx    = [](text key, mut_text, u8)    { return key;    };
        get_output_for_idx_t get_output_for_idx = [](void* output, u8)          { return output; };

        struct number_data_t {
            u8   output_bits      = 8;
            bool output_is_signed = false;

            i64 min = lm::signed_min<64>;
            u64 max = lm::unsigned_max<64>;

            bool allow_decimal = true;
            bool allow_hex     = true;
            bool allow_oct     = true;
            bool allow_binary  = true;
        };
        struct string_data_t {
            st   max_len = 256;
            bool strip   = true;
            enum when_too_large { truncate, error } too_large_behaviour = error;
            bool add_null_terminator = false;

            using size_out_t = void(*)(u8, st);
            size_out_t size_out = nullptr;
        };
        struct enumeration_data_t {
            using parse_t     = field_parse_result(*)(field const&, text, parse_args, text matched_key, u8 key_idx);
            using normalize_t = void(*)(field const&, void* resolved_output, u8 key_idx);
            parse_t     parse     = nullptr;
            normalize_t normalize = nullptr;
        };

        union {
            number_data_t      number_data;
            string_data_t      string_data;
            enumeration_data_t enumeration_data;
        };

        template <typename Enum, int Lo = -128, int Hi = 128>
        static constexpr auto default_enum_parser_for() -> enumeration_data_t::parse_t;
    };

    // ── multikey ──────────────────────────────────────────────────────────────

    struct multikey
    {
        using formatter_t  = field::get_key_for_idx_t;
        using redirector_t = field::get_output_for_idx_t;
        template <typename T>
        using getter_t = T*(*)(u8);

        static constexpr auto no_formatter(text pattern, mut_text, u8) -> text
        { return pattern; }

        static constexpr auto u8_formatter(text pattern, mut_text buf, u8 key_idx) -> text
        { return {buf.data, (st)std::snprintf(buf.data, buf.size, pattern.data, key_idx)}; }

        template <typename Enum, int Lo = -128, int Hi = 128>
        static constexpr auto renum_formatter(text pattern, mut_text buf, u8 key_idx) -> text
        {
            auto re = renum<Enum, Lo, Hi>::unqualified((Enum)key_idx);
            return {buf.data, (st)std::snprintf(buf.data, buf.size, pattern.data, (int)re.size, re.data)};
        }

        template <typename T>
        static constexpr auto default_redirector(void* output, u8 key_idx) -> void*
        { return ((getter_t<T>)output)(key_idx); }

        static constexpr auto no_redirector(void* output, u8) -> void*
        { return output; }

        text        pattern;
        u8          count;
        formatter_t formatter = u8_formatter;
    };

    // ── num ───────────────────────────────────────────────────────────────────

    // Multikey with custom formatter/redirector.
    template <typename Num>
    constexpr auto num(
        multikey mk,
        multikey::getter_t<Num> getter,
        field::number_data_t data = {},
        multikey::redirector_t redirector = multikey::default_redirector<Num>
    ) -> field
    {
        auto ret = field{};
        ret.key    = mk.pattern;
        ret.output = (void*)getter;
        ret.type   = field::number;
        ret.number_data              = data;
        ret.number_data.output_bits  = sizeof(Num) * 8;
        ret.number_data.output_is_signed = veil::is_signed_v<Num>;
        ret.keycount           = mk.count;
        ret.get_key_for_idx    = mk.formatter;
        ret.get_output_for_idx = redirector;
        return ret;
    }

    // Multikey with default u8_formatter.
    template <typename Num>
    constexpr auto num(
        text pattern,
        u8 count,
        multikey::getter_t<Num> getter,
        field::number_data_t data = {}
    ) -> field
    { return num<Num>({.pattern = pattern, .count = count}, getter, data); }

    // Single key, direct reference.
    template <typename Num>
    constexpr auto num(
        text key,
        Num& n,
        field::number_data_t data = {}
    ) -> field
    {
        return num<Num>(
            {.pattern = key, .count = 1, .formatter = multikey::no_formatter},
            (multikey::getter_t<Num>)&n, data, multikey::no_redirector);
    }

    // ── str ───────────────────────────────────────────────────────────────────

    // Multikey with custom formatter/redirector.
    inline auto str(
        multikey mk,
        multikey::getter_t<char> getter,
        field::string_data_t data = {},
        multikey::redirector_t redirector = multikey::default_redirector<char>
    ) -> field
    {
        auto ret = field{};
        ret.key    = mk.pattern;
        ret.output = (void*)getter;
        ret.type   = field::string;
        ret.string_data                  = data;
        ret.string_data.add_null_terminator = true;
        ret.keycount           = mk.count;
        ret.get_key_for_idx    = mk.formatter;
        ret.get_output_for_idx = redirector;
        return ret;
    }

    // Multikey with default u8_formatter.
    inline auto str(
        text pattern,
        u8 count,
        multikey::getter_t<char> getter,
        field::string_data_t data = {}
    ) -> field
    { return str({.pattern = pattern, .count = count}, getter, data); }

    // Single key, char* (size must be in data.max_len).
    inline auto str(
        text key,
        char* out,
        field::string_data_t data = {}
    ) -> field
    {
        return str(
            {.pattern = key, .count = 1, .formatter = multikey::no_formatter},
            (multikey::getter_t<char>)out, data, multikey::no_redirector);
    }

    // Single key, mut_text (size taken from view).
    inline auto str(
        text key,
        mut_text out,
        field::string_data_t data = {}
    ) -> field
    {
        auto ret = field{};
        ret.key    = key;
        ret.output = out.data;
        ret.type   = field::string;
        ret.string_data         = data;
        ret.string_data.max_len = out.size;
        ret.keycount            = 1;
        ret.get_key_for_idx     = multikey::no_formatter;
        ret.get_output_for_idx  = multikey::no_redirector;
        return ret;
    }

    // ── enm ───────────────────────────────────────────────────────────────────

    // Single key.
    template <int Lo = -128, int Hi = 128, typename Enum>
    constexpr auto enm(
        text key,
        Enum& val,
        field::enumeration_data_t::parse_t parser = field::default_enum_parser_for<Enum, Lo, Hi>()
    ) -> field
    {
        auto ret = field{};
        ret.key    = key;
        ret.output = &val;
        ret.type   = field::enumeration;
        ret.enumeration_data.parse = parser;
        ret.keycount           = 1;
        ret.get_key_for_idx    = multikey::no_formatter;
        ret.get_output_for_idx = multikey::no_redirector;
        return ret;
    }

    // ── norm_enm ──────────────────────────────────────────────────────────────
    //
    // Normalized enumeration: parses as EnumPretend, then normalizer converts
    // it in-place to EnumActual. Used for feature (off/on/yes/no/...) etc.
    //
    // normalizer signature: void(field const&, void* resolved_output, u8 key_idx)

    // Single key, direct reference.
    template <
        typename EnumPretend,
        typename EnumActual,
        field::enumeration_data_t::normalize_t Normalizer,
        int Lo = -128,
        int Hi = 128
    >
    constexpr auto norm_enm(
        text key,
        EnumActual& val
    ) -> field
    {
        auto ret = field{};
        ret.key    = key;
        ret.output = &val;
        ret.type   = field::enumeration;
        ret.enumeration_data.parse     = field::default_enum_parser_for<EnumPretend, Lo, Hi>();
        ret.enumeration_data.normalize = Normalizer;
        ret.keycount           = 1;
        ret.get_key_for_idx    = multikey::no_formatter;
        ret.get_output_for_idx = multikey::no_redirector;
        return ret;
    }

    // Multikey with default u8_formatter.
    template <
        typename EnumPretend,
        typename EnumActual,
        field::enumeration_data_t::normalize_t Normalizer,
        int Lo = -128,
        int Hi = 128
    >
    constexpr auto norm_enm(
        text pattern,
        u8 count,
        multikey::getter_t<EnumActual> getter
    ) -> field
    {
        auto ret = field{};
        ret.key    = pattern;
        ret.output = (void*)getter;
        ret.type   = field::enumeration;
        ret.enumeration_data.parse     = field::default_enum_parser_for<EnumPretend, Lo, Hi>();
        ret.enumeration_data.normalize = Normalizer;
        ret.keycount           = count;
        ret.get_key_for_idx    = multikey::u8_formatter;
        ret.get_output_for_idx = multikey::default_redirector<EnumActual>;
        return ret;
    }

    // Multikey with custom formatter/redirector.
    template <
        typename EnumPretend,
        typename EnumActual,
        field::enumeration_data_t::normalize_t Normalizer,
        int Lo = -128,
        int Hi = 128
    >
    constexpr auto norm_enm(
        multikey mk,
        multikey::getter_t<EnumActual> getter,
        multikey::redirector_t redirector = multikey::default_redirector<EnumActual>
    ) -> field
    {
        auto ret = field{};
        ret.key    = mk.pattern;
        ret.output = (void*)getter;
        ret.type   = field::enumeration;
        ret.enumeration_data.parse     = field::default_enum_parser_for<EnumPretend, Lo, Hi>();
        ret.enumeration_data.normalize = Normalizer;
        ret.keycount           = mk.count;
        ret.get_key_for_idx    = mk.formatter;
        ret.get_output_for_idx = redirector;
        return ret;
    }

    // ──  feature (normalized enumeration)  ──────────────────────────────────────────────────────────────
    // Parses as config_ini::feature (disabled/enabled/no/yes/off/on),
    // then normalizes in-place to lm::feature (off/on).

    namespace detail
    {
        constexpr auto feature_normalizer(ini::field const&, void* out, u8) -> void
        { *(lm::feature*)out = to_feature(*(feature*)out); }
    }

    // Single key, direct reference.
    constexpr auto feat(text key, lm::feature& f) -> ini::field
    { return ini::norm_enm<feature, lm::feature, detail::feature_normalizer, 0, 5>(key, f); }

    // Multikey, default u8_formatter.
    constexpr auto feat(
        text pattern,
        u8 count,
        multikey::getter_t<lm::feature> getter
    ) -> ini::field
    { return ini::norm_enm<feature, lm::feature, detail::feature_normalizer, 0, 5>(pattern, count, getter); }

    // Multikey, custom formatter (e.g. renum_formatter for named log levels).
    constexpr auto feat(
        multikey mk,
        multikey::getter_t<lm::feature> getter
    ) -> ini::field
    { return ini::norm_enm<feature, lm::feature, detail::feature_normalizer, 0, 5>(mk, getter); }

    // ── parse ─────────────────────────────────────────────────────────────────
    //
    // An ini file is a string of [section] headers and key = value pairs.
    //
    // Sections prefix every key beneath them: [usb.midi] makes "cable_count"
    // match the field "usb.midi.cable_count". An empty section [] or a key
    // before any section header has no prefix.
    //
    // Values come in three forms:
    //   1. raw     — everything after '=' until ';' or '\n', stripped.
    //   2. quoted  — "…" — contents taken verbatim, no escape sequences.
    //   3. paren   — (…) — nested parens tracked, comments not stripped.
    //
    // Duplicate keys are parsed in order; the last valid write wins.
    // Unknown keys are silently ignored (logged at debug level when log_ignored).
    //
    // keys_to_parse: when non-empty, only these keys are dispatched.
    // keys_to_skip:  when non-empty, these keys are skipped.

    enum class parse_result
    {
        ok,
        sytax_error,
        field_error,
    };

    [[nodiscard]] auto parse(
        text,
        std::span<field const>,
        parse_args = {},
        std::span<text> keys_to_parse = {},
        std::span<text> keys_to_skip  = {}
    ) -> parse_result;
}

// ── field::default_enum_parser_for impl ──────────────────────────────────────

template <typename Enum, int Lo, int Hi>
constexpr auto lm::ini::field::default_enum_parser_for() -> enumeration_data_t::parse_t
{
    return [](ini::field const& field, text input, parse_args args, text matched_key, u8 key_idx) {
        using re = renum<Enum, Lo, Hi>;
        constexpr auto enum_name = type_name<Enum>();

        auto on_exists = [&](auto v) {
            if(!args.dry_run)
            {
                void* out = field.get_output_for_idx(field.output, key_idx);
                *(Enum*)out = v;
                if (field.enumeration_data.normalize)
                    field.enumeration_data.normalize(field, out, key_idx);
            }

            if (!args.log_success) return;
            log::debug(
                "[%.*s - %.*s] = [%.*s]\n",
                (int)matched_key.size, matched_key.data,
                (int)enum_name.size,   enum_name.data,
                (int)input.size,       input.data
            );
        };
        if(re::check_exists(input, on_exists)) return field_parse_result::ok;

        if(!args.log_parse_err) return field_parse_result::enumeration_not_found;

        char fmtbuf[256];
        auto fmtbuf_offset = 0_st;
        for (auto i = 0_st; i < re::count; ++i)
        {
            auto val    = re::values[i] | sc<Enum>;
            auto valstr = re::views[i].unqualified();
            auto fmt    = i == re::count - 1 ? "%.*s" : "%.*s, ";
            auto loglevel = val == *(Enum*)field.get_output_for_idx(field.output, key_idx)
                ? log::level::info
                : log::level::debug;
            auto fmted = log::fmt(
                {fmtbuf + fmtbuf_offset, sizeof(fmtbuf) - fmtbuf_offset},
                log::fmt_t({
                    .fmt       = fmt,
                    .timestamp = log::timestamp_t::no_timestamp,
                    .filename  = log::filename_t::no_filename,
                    .prefix    = lm::feature::off,
                    .loglevel  = loglevel,
                }),
                (int)valstr.size, valstr.data
            );
            fmtbuf_offset += fmted.size;
            if (fmtbuf_offset >= sizeof(fmtbuf)) fmtbuf_offset = sizeof(fmtbuf);
        }

        auto yellow = config.logging.level[log::level::warn].ansi();
        auto gray   = config.logging.level[log::level::debug].ansi();
        auto white  = config.logging.level[log::level::regular].ansi();
        #define LM_WHITE  (int)white.size,white.data
        #define LM_GRAY   (int)gray.size,gray.data
        #define LM_YELLOW (int)yellow.size,yellow.data
        log::warn<128 * 3>(
            "Ignoring %.*s[%.*s%.*s%.*s]%.*s for %.*s[%.*s%.*s(%.*s%.*s)]%.*s\n\t> Allowed values are: %.*s[%.*s%.*s]\n",
           LM_WHITE, LM_GRAY, (int)input.size, input.data, LM_WHITE, LM_YELLOW,
           LM_WHITE, (int)field.key.size, field.key.data, LM_GRAY, (int)enum_name.size, enum_name.data, LM_WHITE, LM_YELLOW,
           LM_WHITE, (int)fmtbuf_offset, fmtbuf, LM_WHITE
        );
        #undef LM_WHITE
        #undef LM_GRAY
        #undef LM_YELLOW

        return field_parse_result::enumeration_not_found;
    };
}
