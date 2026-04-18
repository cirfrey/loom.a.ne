#pragma once

#include "lm/core.hpp"
#include "lm/log.hpp"

namespace lm::ini
{
    enum class parse_result
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
    };
    struct parse_args
    {
        bool log_success = true;
        bool log_ignored = true;
    };

    // TODO: sure would be nice to have a printable description on error.
    struct field
    {
        [[nodiscard]] auto parse(text, parse_args = parse_args{}) -> parse_result;

        text key = {nullptr, 0};
        void* output = nullptr;

        enum type_t {
            none,   // Mark as unused/not-initialized.
            number, // Make sure to set number_data.variable_size and number_data.variable_is_signed,
                    // otherwise your memory WILL get corrupted.
            string, // Expects .variable to be a char[string_data.max_len], null terminated.
            enumeration,
        } type = none;

        struct number_data_t {
            u8   output_bits      = 8;
            bool output_is_signed = false;

            // .variable_size and .variable_is_signed take priority over these values.
            i64 min = lm::signed_min<64>;
            u64 max = lm::unsigned_max<64>;

            bool allow_hex    = true;
            bool allow_oct    = true;
            bool allow_binary = true;
        };
        struct string_data_t {
            st max_len = 256;
            bool strip = true;
        };
        struct enumeration_data_t {
            using parse_t = parse_result(*)(field& field, text input, parse_args);
            // Given a lm::text, writes to .output if valid, otherwise should print
            // the valid enumerations for the user to debug.
            // - true = parsed successfully
            // - false = :(
            // When nullptr, warning is logged and field is skipped.
            parse_t parse = nullptr;

            using normalize_t = void(*)(field& field);
            normalize_t normalize = nullptr;
        };

        union
        {
            number_data_t      number_data;
            string_data_t      string_data;
            enumeration_data_t enumeration_data;
        };

        // Checks via lm::renum.
        template <typename Enum, int Lo = -128, int Hi = 128>
        static constexpr auto default_enum_parser_for() -> enumeration_data_t::parse_t;
    };

    template <typename Num>
    constexpr auto number_field(text key, Num& num, field::number_data_t data) -> field
    {
        auto ret = field{};
        ret.key = key;
        ret.output = &num;
        ret.type = field::number;
        ret.number_data = data;
        ret.number_data.output_bits      = sizeof(Num) * 8;
        ret.number_data.output_is_signed = veil::is_signed_v<Num>;
        return ret;
    }

    //constexpr auto string_field()

    template <
        typename EnumPretend,
        typename EnumActual,
        field::enumeration_data_t::normalize_t Normalizer,
        int Lo = -128,
        int Hi = 128
    >
    constexpr auto normalized_enumeration_field(
        text key,
        EnumActual& val
    ) -> field
    {
        auto ret = field{};
        ret.key = key;
        ret.output = &val;
        ret.type = field::enumeration;
        ret.enumeration_data.normalize = Normalizer;
        ret.enumeration_data.parse     = field::default_enum_parser_for<EnumPretend, Lo, Hi>();
        return ret;
    }

    template <int Lo = -128, int Hi = 128, typename Enum>
    constexpr auto enumeration_field(
        text key,
        Enum& val,
        field::enumeration_data_t::parse_t parser =
            field::default_enum_parser_for<Enum, Lo, Hi>()
    ) -> field
    {
        auto ret = field{};
        ret.key = key;
        ret.output = &val;
        ret.type = field::enumeration;
        ret.enumeration_data.parse = parser;
        return ret;
    }
}

// Impls.
template <typename Enum, int Lo, int Hi>
constexpr auto lm::ini::field::default_enum_parser_for() -> enumeration_data_t::parse_t
{
    return [](ini::field& field, text input, parse_args args){
        using re = renum<Enum, Lo, Hi>;
        constexpr auto enum_name = type_name<Enum>();

        auto on_exists = [&](auto v){
            *(Enum*)field.output = v;
            if(field.enumeration_data.normalize)
                field.enumeration_data.normalize(field);
            if(!args.log_success) return;
            log::debug(
                "[%.*s - %.*s] = [%.*s]\n",
                (int)field.key.size, field.key.data,
                (int)enum_name.size, enum_name.data,
                (int)input.size,     input.data
            );
        };
        if(re::check_exists(input, on_exists)) return parse_result::ok;

        if(!args.log_ignored) return parse_result::enumeration_not_found;

        char fmtbuf[256];
        auto fmtbuf_offset = 0;
        for(auto i = 0; i < re::count; ++i)
        {
            auto val    = re::values[i] | sc<Enum>;
            auto valstr = re::views[i].unqualified();
            auto fmt    = i == re::count - 1
                ? "%.*s"
                : "%.*s, ";
            auto loglevel = val == *(Enum*)field.output
                ? log::level::info
                : log::level::debug;
            auto fmted = log::fmt(
                {fmtbuf + fmtbuf_offset, sizeof(fmtbuf) - fmtbuf_offset},
                log::fmt_t({
                    .fmt       = fmt,
                    .timestamp = log::no_timestamp,
                    .filename  = log::no_filename,
                    .loglevel  = loglevel,
                }),
                (int)valstr.size, valstr.data
            );
            fmtbuf_offset += fmted.size;
            if(fmtbuf_offset >= sizeof(fmtbuf)) fmtbuf_offset = sizeof(fmtbuf);
        }

        auto const& yellow = config.logging.level_ansi[log::level::warn];
        auto const& gray   = config.logging.level_ansi[log::level::debug];
        auto const& white  = config.logging.level_ansi[log::level::regular];
        log::warn<128 * 3>(
            "Ignoring %.*s[%.*s]%.*s for %.*s[%.*s - %.*s]%.*s\n\t> Allowed values are: %.*s[%.*s%.*s]\n",
            (int)white.size, white.data, (int)input.size,     input.data,     (int)yellow.size, yellow.data,
            (int)white.size, white.data, (int)field.key.size, field.key.data, (int)enum_name.size, enum_name.data, (int)yellow.size, yellow.data,
            (int)white.size, white.data, (int)fmtbuf_offset,  fmtbuf,         (int)white.size, white.data
        );

        return parse_result::enumeration_not_found;
    };
}
