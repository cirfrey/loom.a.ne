#pragma once

#include "lm/core.hpp"
#include "lm/log.hpp"

namespace lm::ini
{
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
    };
    struct parse_args
    {
        bool log_success = true;
        bool log_error   = true;
        bool log_ignored = true;

        bool match_only_one_field = false;
    };

    // TODO: sure would be nice to have a printable description on error.
    struct field
    {
        [[nodiscard]] auto parse(text, parse_args = parse_args{}, text matched_key = {}, u8 key_idx = 1) const -> field_parse_result;

        text key = {nullptr, 0};
        void* output = nullptr;

        enum type_t {
            none,   // Mark as unused/not-initialized.
            number, // Make sure to set number_data.variable_size and number_data.variable_is_signed,
                    // otherwise your memory WILL get corrupted.
            string, // Expects .variable to be a char[string_data.max_len], null terminated.
            enumeration,
        } type = none;

        u8 keycount = 1;
        // Only necessary to override if keycount > 1.
        using get_key_for_idx_t = text(*)(text key, mut_text buf, u8 key_idx);
        get_key_for_idx_t get_key_for_idx = [](text key, mut_text, u8){ return key; };
        // Only necessary to override if keycount > 1.
        using get_output_for_idx_t = void*(*)(void* output, u8 key_idx);
        get_output_for_idx_t get_output_for_idx = [](void* output, u8){ return output; };

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
            enum when_too_large
            {
                truncate,
                error,
            } too_large_behaviour = error;
            bool add_null_terminator = false;

            using size_out_t = void(*)(u8, st);
            size_out_t size_out = nullptr;
        };
        struct enumeration_data_t {
            using parse_t = field_parse_result(*)(field const& field, text input, parse_args, u8 key_idx);
            // Given a lm::text, writes to .output if valid, otherwise should print
            // the valid enumerations for the user to debug.
            // - true = parsed successfully
            // - false = :(
            // When nullptr, warning is logged and field is skipped.
            parse_t parse = nullptr;

            using normalize_t = void(*)(field const& field);
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

    struct key_info_t
    {
        text key = {};
        u8 keycount = 1;
        field::get_key_for_idx_t key_formatter = nullptr;
        field::get_output_for_idx_t output_redirector = nullptr;
    };

    template <typename Num>
    constexpr auto number_field_raw(key_info_t key, void* output, field::number_data_t data) -> field
    {
        auto ret = field{};
        ret.key = key.key;
        ret.output = output;
        ret.type = field::number;
        ret.number_data = data;
        ret.number_data.output_bits      = sizeof(Num) * 8;
        ret.number_data.output_is_signed = veil::is_signed_v<Num>;

        ret.keycount = key.keycount;
        if(key.key_formatter)     ret.get_key_for_idx    = key.key_formatter;
        if(key.output_redirector) ret.get_output_for_idx = key.output_redirector;

        return ret;
    }

    template <typename Num>
    constexpr auto number_field(key_info_t key, Num& num, field::number_data_t data) -> field
    { return number_field_raw<Num>(key, &num, data); }

    constexpr auto string_field(key_info_t key, mut_text str, field::string_data_t data) -> field
    {
        auto ret = field{};
        ret.key = key.key;
        ret.output = str.data;
        ret.type = field::string;
        ret.string_data = data;
        ret.string_data.max_len = str.size;

        ret.keycount = key.keycount;
        if(key.key_formatter)     ret.get_key_for_idx    = key.key_formatter;
        if(key.output_redirector) ret.get_output_for_idx = key.output_redirector;

        return ret;
    }

    // In this case you *need* to supply the size.
    constexpr auto string_field_raw(key_info_t key, void* out, field::string_data_t data) -> field
    {
        auto ret = field{};
        ret.key = key.key;
        ret.output = out;
        ret.type = field::string;
        ret.string_data = data;
        ret.string_data.add_null_terminator = true;

        ret.keycount = key.keycount;
        if(key.key_formatter)     ret.get_key_for_idx    = key.key_formatter;
        if(key.output_redirector) ret.get_output_for_idx = key.output_redirector;

        return ret;
    }

    // In this case you *need* to supply the size.
    constexpr auto string_field(key_info_t key, char* str, field::string_data_t data) -> field
    {
        return string_field_raw(key, str, data);
    }

    template <
        typename EnumPretend,
        typename EnumActual,
        field::enumeration_data_t::normalize_t Normalizer,
        int Lo = -128,
        int Hi = 128
    >
    constexpr auto normalized_enumeration_field_raw(
        key_info_t key,
        void* output
    ) -> field
    {
        auto ret = field{};
        ret.key = key.key;
        ret.output = output;
        ret.type = field::enumeration;
        ret.enumeration_data.normalize = Normalizer;
        ret.enumeration_data.parse     = field::default_enum_parser_for<EnumPretend, Lo, Hi>();

        ret.keycount = key.keycount;
        if(key.key_formatter)     ret.get_key_for_idx    = key.key_formatter;
        if(key.output_redirector) ret.get_output_for_idx = key.output_redirector;

        return ret;
    }

    template <
        typename EnumPretend,
        typename EnumActual,
        field::enumeration_data_t::normalize_t Normalizer,
        int Lo = -128,
        int Hi = 128
    >
    constexpr auto normalized_enumeration_field(
        key_info_t key,
        EnumActual& val
    ) -> field
    { return normalized_enumeration_field_raw<EnumPretend, EnumActual, Normalizer, Lo, Hi>(key, &val); }



    template <int Lo = -128, int Hi = 128, typename Enum>
    constexpr auto enumeration_field(
        key_info_t key,
        Enum& val,
        field::enumeration_data_t::parse_t parser =
            field::default_enum_parser_for<Enum, Lo, Hi>()
    ) -> field
    {
        auto ret = field{};
        ret.key = key.key;
        ret.output = &val;
        ret.type = field::enumeration;
        ret.enumeration_data.parse = parser;

        ret.keycount = key.keycount;
        if(key.key_formatter)     ret.get_key_for_idx    = key.key_formatter;
        if(key.output_redirector) ret.get_output_for_idx = key.output_redirector;

        return ret;
    }

    using parse_result = field_parse_result;
    // The ini parser proper.
    // For our intents and purposes an ini file is a string that:
    // - Has [sections]
    // - Has [key] = [value]
    //
    // A [section] is treated as a key modifier, that is, a [usb.string_descriptor] prefixes every
    // [key] under it with [usb.string_descriptor.${key}]. To not add any prefix you either declare
    // at the top of the string where there are no prefixes or add an empty section [] like so (ugly).
    //
    // A [key] is any string of characters that precedes an equals sign, they can be letters, numbers,
    // symbols, whatever, anything goes as long as its not an equal sign [=]. Keys are string.strip()'ed.
    // The correct parser for a key (string, number, enum) is decided given on the fields passed to this function.
    //
    // A [value] is any of the following three:
    // - 1. Some sequence of characters after the equal sign [=] until the end of the line [\n].
    // - 2. Some sequence of characters prefixed with quotes ["] until the next quote ["].
    // - 3. Some sequence of characters prefixed with an open bracket [(] until *corresponding* closing brace [)].
    // In case [1.] the value is string.strip()'ed before being fed into the corresponding parser.
    // In case [2.] and [3.] the prefixes and postfixes are stripped before being fed into the corresponding parser.
    //
    // Some other misc notes:
    // - Duplicate keys are allowed, they are all parsed in order, therefore the last one will hold the final result (if parsed correctly).
    // - Comments are anything that precedes [;] until the end of the line [\n] and are NOT in a [2.] or [3.] value,
    //   On those cases, they are treated as part of the value.
    //
    // The job of this function is to break the input text into [key] = [value] pairs, identify the [key] and [value]
    // while keeping track of the [section]s and forward the [value] in the corresponding field, given the [key].
    //
    // NOTE: When set, only keys in keys_to_parse are parsed. The rest are ignored but not treated as errors.
    // NOTE: keys_to_skip does the opposite.
    [[nodiscard]] auto parse(
        text,
        std::span<field const>,
        parse_args = {},
        std::span<text> keys_to_parse = {},
        std::span<text> keys_to_skip = {}
    ) -> parse_result;
}

// Impls.
template <typename Enum, int Lo, int Hi>
constexpr auto lm::ini::field::default_enum_parser_for() -> enumeration_data_t::parse_t
{
    // TODO: implement key_idx.
    return [](ini::field const& field, text input, parse_args args, u8 key_idx){
        using re = renum<Enum, Lo, Hi>;
        constexpr auto enum_name = type_name<Enum>();

        auto on_exists = [&](auto v){
            void* out = field.output;
            if(key_idx >= 1) out = field.get_output_for_idx(out, key_idx);

            *(Enum*)out = v;
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
        if(re::check_exists(input, on_exists)) return field_parse_result::ok;

        if(!args.log_error) return field_parse_result::enumeration_not_found;

        char fmtbuf[256];
        auto fmtbuf_offset = 0_st;
        for(auto i = 0_st; i < re::count; ++i)
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
                    .timestamp = log::timestamp_t::no_timestamp,
                    .filename  = log::filename_t::no_filename,
                    .prefix    = feature::off,
                    .loglevel  = loglevel,
                }),
                (int)valstr.size, valstr.data
            );
            fmtbuf_offset += fmted.size;
            if(fmtbuf_offset >= sizeof(fmtbuf)) fmtbuf_offset = sizeof(fmtbuf);
        }

        // TODO: this needs to use raw ansi and not level_ansi.
        // auto yellow = config.logging.level.data[log::level::warn];
        // auto gray   = config.logging.level.data[log::level::debug];
        // auto white  = config.logging.level.data[log::level::regular];
        // log::warn<128 * 3>(
        //     "Ignoring %s[%s%.*s%s]%s for %s[%.*s%s(%.*s%s)]%s\n\t> Allowed values are: %s[%.*s%s]\n",
        //     white, gray, (int)input.size, input.data, white, yellow,
        //     white, (int)field.key.size, field.key.data, gray, (int)enum_name.size, enum_name.data, white, yellow,
        //     white, (int)fmtbuf_offset, fmtbuf, white
        // );

        return field_parse_result::enumeration_not_found;
    };
}
