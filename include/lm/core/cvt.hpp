// Loomane Core: Type Conversion Engine (cvt).
// * An utility for expressive, zero-cost type transformations.
// This header provides a way to treat casts as data-flow operations
// rather than using clunky C-style syntax.
// * E.g: 5.8f | to<int>            => will return `int{5}`.
// * E.g: int x = 4.8f + 3.2f | toe => will compile and `x == 8` (+ has higher priority than |).
// * E.g: to<int>(5.8f)             => will return `int{5}`.
#pragma once

#include "lm/core/veil.hpp"

namespace lm::cvt::detail
{
    // Base for stateless: Zero size thanks to empty base optimization
    template <typename F, bool IsEmpty = veil::is_empty_v<F>>
    struct storage_node {
        constexpr storage_node(F&&) {}
        constexpr const F& get() const { return *reinterpret_cast<const F*>(this); }
    };

    // Base for stateful: Must actually store the object
    template <typename F>
    struct storage_node<F, false> {
        F op;
        constexpr storage_node(F&& f) : op(veil::forward<F>(f)) {}
        constexpr const F& get() const { return op; }
    };
}

namespace lm::cvt
{
    // The Converter Shell.
    // Wraps a functional logic block (UnderlyingConverter) and attaches
    // "Modes" to it via mixin inheritance. These modes dictate how the
    // converter can be used (e.g., via math operators, pipes, or function calls).
    // Don't too much about how exactly it works, it just does.
    template <typename UnderlyingConverter, template <typename, typename> typename... Modes>
    struct converter :
        detail::storage_node<UnderlyingConverter>,
        Modes<UnderlyingConverter, converter<UnderlyingConverter, Modes...>>...
    {
        using storage = detail::storage_node<UnderlyingConverter>;

        constexpr converter(UnderlyingConverter&& func)
            : storage(veil::forward<UnderlyingConverter>(func)) {}

        template <template <typename, typename> typename M>
        static constexpr bool allows_cvt_mode = (veil::is_same_v<M<void, void>, Modes<void, void>> || ...);
    };

    // Operation Modes.
    // These empty structs are mixed into the converter to enable specific syntaxes.
    // Because they are empty, the compiler applies Empty Base Class Optimization (EBCO),
    // ensuring the final converter object has a zero-byte footprint.

    // Enables arithmetic operators (*, /, +, -) to trigger the conversion.
    template <typename UnderlyingConverter, typename Shell> struct mode_math {};
    // Enables the pipe operator (|) for data-flow style conversions.
    template <typename UnderlyingConverter, typename Shell> struct mode_pipe {};
    // Enables standard function call syntax: cvt::to<u8>(value).
    template <typename UnderlyingConverter, typename Shell> struct mode_fncall {
        template <typename From> constexpr auto operator()(From&& from) const;
    };

    // Converter Factory.
    // * Creates a custom converter from any lambda or function object.
    // * Usage: (Also refer to the examples below)
    // auto to_hex = cvt::make_custom<mode_pipe>([](u8 v) { ... });
    // auto str    = raw_byte | to_hex;
    template < template <typename, typename> typename... SupportedModes >
    constexpr auto make_custom(auto&& func) {
        // Deduces the type of the lambda/function and initializes the shell
        return converter< veil::decay_t<decltype(func)>, SupportedModes... >{
            veil::forward<decltype(func)>(func)
        };
    }
}

namespace lm
{
    // Standard Casts.
    // * Defaults to mode_fncall and mode_pipe for safety.
    // This avoids common arithmetic precedence pitfalls while keeping
    // the code expressive.
    // * Examples:
    // i32 x = 5.5f | cvt::to<i32>;  // Pipe style
    // i32 y = cvt::to<i32>(5.5f);   // Function style
    template <typename To> inline constexpr auto sc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return static_cast<To>(veil::forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto dc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return dynamic_cast<To>(veil::forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto rc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return reinterpret_cast<To>(veil::forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto cc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return const_cast<To>(veil::forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto bc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return bit_cast<To>(veil::forward<decltype(from)>(from)); });
    // You probably want to use this one for most cases, it just wraps to static_cast.
    template <typename To> inline constexpr auto to = sc<To>;

    // Very funny haha.
    // Use this for type punning and playing a joke on the type system.
    template <typename To>
    inline constexpr auto pun = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>(
        [](auto&& from) constexpr -> To {
            using From = veil::remove_cvref_t<decltype(from)>;

            static_assert(sizeof(From) <= sizeof(To),
                "This vessel is much to small for the smuggled goods milord.");

            union {
                From in;
                To out;
            } u = {};

            u.in = veil::forward<decltype(from)>(from);// Designated initializer for clarity
            return u.out;
        }
    );
    template <typename To>
    inline constexpr auto smuggle = pun<To>;

    // Mutate something in place. Returns a mutator that you can use in pipelines
    // or just call directly on the thing you desire to mutate.
    constexpr auto mut(auto&& op) {
        return cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>(
            [op = veil::forward<decltype(op)>(op)](auto& value) constexpr -> auto& {
                (op(value));
                return value;
            }
        );
    }

    // Transform a value using a custom operation.
    // Unlike mut, this returns a NEW value (the result of the op).
    constexpr auto trans(auto&& op) {
        return cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>(
            [op = veil::forward<decltype(op)>(op)](auto&& value) constexpr -> decltype(auto) {
                // Perfect forwarding into the operator, and returning the result
                return op(veil::forward<decltype(value)>(value));
            }
        );
    }
}

// --- Mode implementation Details ---

namespace lm::cvt
{
    template <typename UC, typename Shell>
    template <typename From>
    constexpr auto mode_fncall<UC, Shell>::operator()(From&& from) const
    {
        // Downcast to the actual converter to access the stored lambda/function
        return static_cast<const Shell*>(this)->get()(veil::forward<From>(from));
    }

    #define LM_CVT_IMPL_ARITHM_OP_L(MODE, OP) \
        template <typename T, typename UC, template <typename, typename> typename... Modes> \
        requires (converter<UC, Modes...>::template allows_cvt_mode< MODE >) \
        constexpr auto OP (const converter<UC, Modes...>& cvt, T&& val) { \
            return cvt.get()(veil::forward<T>(val)); \
        }
    #define LM_CVT_IMPL_ARITHM_OP_R(MODE, OP) \
        template <typename T, typename UC, template <typename, typename> typename... Modes> \
        requires (converter<UC, Modes...>::template allows_cvt_mode< MODE >) \
        constexpr auto OP (T&& val, const converter<UC, Modes...>& cvt) { \
            return cvt.get()(veil::forward<T>(val)); \
        }

    /* --- mode_pipe  --- */
    LM_CVT_IMPL_ARITHM_OP_R(mode_pipe, operator|)

    /* --- mode_math --- */
    LM_CVT_IMPL_ARITHM_OP_L(mode_math, operator*)
    LM_CVT_IMPL_ARITHM_OP_R(mode_math, operator*)
    LM_CVT_IMPL_ARITHM_OP_L(mode_math, operator/)
    LM_CVT_IMPL_ARITHM_OP_R(mode_math, operator/)
    LM_CVT_IMPL_ARITHM_OP_L(mode_math, operator+)
    LM_CVT_IMPL_ARITHM_OP_R(mode_math, operator+)
    LM_CVT_IMPL_ARITHM_OP_L(mode_math, operator-)
    LM_CVT_IMPL_ARITHM_OP_R(mode_math, operator-)
}

// Internal logic for cvt::toe.
// Uses a proxy struct (implicit_cvt) to delay the cast until the compiler
// determines the ExpectedType during assignment or function passing.
namespace lm
{
    struct expected {
        template <typename From>
        struct implicit_cvt
        {
            From v;

            template<typename ExpectedType>
            constexpr operator ExpectedType() const &
            { return v | to<ExpectedType>; }

            template <typename ExpectedType>
            constexpr operator ExpectedType() &&
            { return veil::move(v) | to<ExpectedType>; }
        };

        template <typename From>
        constexpr auto operator()(From&& from) const
        { return implicit_cvt<From>{ veil::forward<From>(from) }; }
    };

    // The "Expected" Converter.
    // * A specialized "Magic" converter that infers the required destination type
    // from the assignment context / function return type.
    // * Example:
    // u16 value = 500.0f | cvt::toe; // Compiler infers u16
    inline constexpr auto toe          = cvt::converter< expected, cvt::mode_fncall, cvt::mode_pipe >{ expected{} };
    inline constexpr auto to_expected  = cvt::converter< expected, cvt::mode_fncall, cvt::mode_pipe >{ expected{} };
}
