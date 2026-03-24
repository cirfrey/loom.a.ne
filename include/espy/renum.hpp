#pragma once

#include <string_view>
#include <cstddef>

namespace espy::renum
{
    template <typename Enum, typename I, I... Values>
    struct reflected;
}

namespace espy::renum::detail
{
    // Minimal internal implementation of remove_cvref to avoid <type_traits>
    template<typename T> struct remove_cv      { using type = T; };
    template<typename T> struct remove_cv<const T> { using type = T; };
    template<typename T> struct remove_cv<volatile T> { using type = T; };
    template<typename T> struct remove_cv<const volatile T> { using type = T; };
    template<typename T> struct remove_cvref { using type = typename remove_cv<T>::type; };
    template<typename T> struct remove_cvref<T&> { using type = typename remove_cv<T>::type; };
    template<typename T> struct remove_cvref<T&&> { using type = typename remove_cv<T>::type; };


    struct view
    {
        const char* begin;
        std::size_t len;

        constexpr operator std::string_view() const { return qualified(); }

        constexpr auto qualified() const -> std::string_view
        { return {begin, len}; }

        constexpr auto unqualified() const -> std::string_view
        {
            auto q = qualified();
            auto last_colon = 0u;
            for(auto i = 0u; i < q.size(); ++i) { if(q[i] == ':') last_colon = i + 1; }
            q.remove_prefix(last_colon);
            return q;
        }

        constexpr auto semi_qualified() const -> std::string_view
        {
            const auto unq = unqualified();
            auto data = unq.data();
            auto size = unq.size();

            #if defined(_MSC_VER)
                for(auto colons = 0u; colons < 2; colons += *(--data) == ':') ++size;
                --data;
                while(*data != ',' && *data != ':') { --data; ++size; }
                ++size;
            #else
                for(auto colons = 0u; colons <= 2 && *data != ' '; colons += *(--data) == ':') ++size;
            #endif
            return {data + 1, size - 1};
        }
    };

    template <typename Enum, Enum Value>
    consteval auto enum_value_view()
    {
        #if defined(_MSC_VER)
            std::string_view name = __FUNCSIG__;
            std::string_view prefix = "auto __cdecl espy::renum::detail::enum_value_view<";
            std::string_view suffix = ">(void)";
            name.remove_prefix(prefix.size());
            name.remove_suffix(suffix.size());

            const char* fn = name.data();
            auto len = name.length();
            auto open_brackets = 0u;
            while(true) {
                ++fn; --len;
                if(*fn == '<') open_brackets++;
                if(*fn == '>') open_brackets--;
                if(*fn == ',' && open_brackets == 0) break;
            }
            ++fn; --len;
            if(*fn == '(') return view{nullptr, 0};
            return view{fn, len};
        #else
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
        #endif
    }

    template <typename I, I... Is> struct seq {
        // This is the missing link that connects the range check to the reflected class
        template <typename Enum>
        using apply = reflected<Enum, I, Is...>;
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

namespace espy::renum
{
    template <typename Enum, typename I, I... Values>
    struct reflected
    {
        static_assert(sizeof...(Values) > 0, "No valid enum values found in range.");

        static constexpr auto count = sizeof...(Values);
        static constexpr I values[] = { Values... };
        static constexpr detail::view views[] = { detail::enum_value_view<Enum, Enum(Values)>()... };

        constexpr auto operator[] (Enum const& e) const {
            for(auto i = 0u; i < count; ++i) { if(values[i] == I(e)) return views[i]; }
            return detail::view{"INVALID_ENUM", 12};
        }

        struct value_iterator_v { I value; detail::view view; };
        template <bool IsValue> struct iterator {
            std::size_t i = 0;
            constexpr auto operator*() const { return value_iterator_v{ values[i], views[i] }; }
            constexpr auto operator++() { ++i; return *this; }
            constexpr bool operator!=(std::nullptr_t) const { return i != count; }
            constexpr iterator begin() const { return *this; }
            constexpr std::nullptr_t end() const { return nullptr; }
        };
    };

    // Helper to auto-generate the reflection
    template <typename Enum, int Lo = -128, int Hi = 128>
    using reflect = typename decltype(detail::enum_check_range_impl<Enum, int, Lo, Hi>())::template apply<Enum>;
}
