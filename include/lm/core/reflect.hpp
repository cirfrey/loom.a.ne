#pragma once

#include "lm/core/types.hpp"
#include "lm/core/veil.hpp"
#include "lm/core/helpers.hpp"

namespace lm
{
    // from https://stackoverflow.com/questions/81870/is-it-possible-to-print-a-variables-type-in-standard-c/56766138#56766138
    // Thanks!
    template<typename T, bool EnableShortInts = true>
    constexpr auto type_name() -> text
    {
        if constexpr (EnableShortInts && veil::is_integral_v<T>)
        {
            constexpr char const* unsigned_atlas[] = {"u8", "u16", "dummy", "u32", "dummy", "dummy", "dummy", "u64"};
            constexpr char const* signed_atlas[]   = {"i8", "i16", "dummy", "i32", "dummy", "dummy", "dummy", "i64"};

            if constexpr (veil::is_signed_v<T>){ return to_text(signed_atlas[sizeof(T) - 1]); }
            else return to_text(unsigned_atlas[sizeof(T) - 1]);
        }
        text name, prefix, suffix;
        #if defined(__clang__)
            name   = to_text(__PRETTY_FUNCTION__);
            prefix = to_text("lm::text lm::type_name() [T = ");
            if constexpr (EnableShortInts) { suffix = to_text(", EnableShortInts = true]"); }
            else                           { suffix = to_text(", EnableShortInts = false]"); }
        #elif defined(__GNUC__)
            name   = to_text(__PRETTY_FUNCTION__);
            prefix = to_text("constexpr lm::text lm::type_name() [with T = ");
            if constexpr (EnableShortInts) { suffix = to_text("; bool EnableShortInts = true]"); }
            else                           { suffix = to_text("; bool EnableShortInts = false]"); }
        #elif defined(_MSC_VER)
            static_assert(false, "Please implement type_name() for this compiler");
        #else
            static_assert(false, "Please implement type_name() for this compiler");
        #endif
        // Remove prefix.
        name.data += prefix.size;
        name.size -= prefix.size;
        // Remove suffix.
        name.size -= suffix.size;
        return name;
    }

    template <typename T, bool EnableShortInts = true>
    constexpr auto type_name(T&&) -> text
    { return type_name< T, EnableShortInts >(); }

    template <typename T, bool EnableShortInts = true>
    constexpr auto clean_type_name(T&&) -> text
    { return type_name< veil::remove_cvref_t<T>, EnableShortInts >(); }

    template <typename Enum, typename I, I... Values>
    struct renum_t;
}

/// Renum detail
namespace lm::detail
{
    struct view
    {
        char const* begin;
        st len;

        constexpr operator text() const { return qualified(); }

        constexpr auto qualified() const -> text
        { return {.data = begin, .size = len}; }

        constexpr auto unqualified() const -> text
        {
            auto q = qualified();
            auto last_colon = 0u;
            for(auto i = 0u; i < q.size; ++i) { if(q.data[i] == ':') last_colon = i + 1; }
            q.data += last_colon;
            q.size -= last_colon;
            return q;
        }

        constexpr auto semi_qualified() const -> text
        {
            const auto unq = unqualified();
            auto data = unq.data;
            auto size = unq.size;

            #if defined(_MSC_VER)
                for(auto colons = 0u; colons < 2; colons += *(--data) == ':') ++size;
                --data;
                while(*data != ',' && *data != ':') { --data; ++size; }
                ++size;
            #else
                for(auto colons = 0u; colons <= 2 && *data != ' '; colons += *(--data) == ':') ++size;
            #endif
            return {.data = data + 1, .size = size - 1};
        }
    };

    template <typename Enum, Enum Value>
    consteval auto enum_value_view()
    {
        #if !defined(_MSC_VER)
            const char* fn = __PRETTY_FUNCTION__;
            #if defined(__clang__)
                while(*(fn++) != ',');
            #elif defined(__GNUC__)
                while(*(fn++) != ';');
            #endif
            while(*(fn++) != '=');
            fn += 1;
            if(*fn == '(') return view{nullptr, 0};
            auto len = 0u;
            while(*(fn + len) != ']') ++len;
            return view{fn, len};
        #else
            static_assert(false, "Please implement enum_value_view() for this compiler");
        #endif
    }

    template <typename I, I... Is> struct seq {
        // This is the missing link that connects the range check to the renum_t class
        template <typename Enum>
        using apply = renum_t<Enum, I, Is...>;
    };

    template <typename Enum, typename I, I Lo, I Hi, I... Acc>
    consteval auto enum_check_range_impl()
    {
        if constexpr(Lo > Hi) return seq<I, Acc...>{};
        else if constexpr(enum_value_view<Enum, Enum(Lo)>().begin != nullptr)
            return enum_check_range_impl<Enum, I, Lo+1, Hi, Acc..., Lo>();
        else
            return enum_check_range_impl<Enum, I, Lo+1, Hi, Acc...>();
    }
}

namespace lm
{
    template <typename Enum, typename I, I... Values>
    struct renum_t
    {
        static_assert(sizeof...(Values) > 0, "No valid enum values found in range.");

        static constexpr auto count = sizeof...(Values);
        static constexpr I values[] = { Values... };
        static constexpr detail::view views[] = { detail::enum_value_view<Enum, Enum(Values)>()... };

        static constexpr auto operator[] (Enum const& e) {
            for(auto i = 0u; i < count; ++i) { if(values[i] == I(e)) return views[i]; }
            return detail::view{"INVALID_ENUM", 12};
        }
        static constexpr auto qualified(Enum const& e)      -> text { return operator[](e).qualified(); }
        static constexpr auto unqualified(Enum const& e)    -> text { return operator[](e).unqualified(); }
        static constexpr auto semi_qualified(Enum const& e) -> text { return operator[](e).semi_qualified(); }

        struct value_iterator_v { I value; detail::view view; };
        template <bool IsValue> struct iterator {
            st i = 0;
            constexpr auto operator*() const { return value_iterator_v{ values[i], views[i] }; }
            constexpr auto operator++() { ++i; return *this; }
            constexpr bool operator!=(void*) const { return i != count; }
            constexpr iterator begin() const { return *this; }
            constexpr void* end() const { return nullptr; }
        };
    };

    // Helper to auto-generate the reflection
    template <typename Enum, int Lo = -128, int Hi = 128>
    using renum = typename decltype(detail::enum_check_range_impl<Enum, int, Lo, Hi>())::template apply<Enum>;
}
