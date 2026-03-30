// Loomane Core: Type Conversion Engine (cvt).
// * An utility for expressive, zero-cost type transformations.
// This header provides a way to treat casts as data-flow operations
// rather than using clunky C-style syntax.
// * E.g: 5.8f | cvt::to<int>            => will return `int{5}`.
// * E.g: int x = 5.8f + 3.2f | cvt::toe => will compile and `x == 5`.
// * E.g: cvt::to<int>(5.8f)             => will return `int{5}`.
#pragma once

#include "lm/core/traits.hpp"

namespace lm::cvt
{
    // The Converter Shell.
    // Wraps a functional logic block (UnderlyingConverter) and attaches
    // "Modes" to it via mixin inheritance. These modes dictate how the
    // converter can be used (e.g., via math operators, pipes, or function calls).
    template <typename UnderlyingConverter, template <typename> typename... SupportedModes >
    struct converter : SupportedModes<UnderlyingConverter>... {};

    // Operation Modes.
    // These empty structs are mixed into the converter to enable specific syntaxes.
    // Because they are empty, the compiler applies Empty Base Class Optimization (EBCO),
    // ensuring the final converter object has a zero-byte footprint.

    // Enables arithmetic operators (*, /, +, -) to trigger the conversion.
    template <typename UnderlyingConverter> struct mode_math {};
    // Enables the pipe operator (|) for data-flow style conversions.
    template <typename UnderlyingConverter> struct mode_pipe {};
    // Enables standard function call syntax: cvt::to<u8>(value).
    template <typename UnderlyingConverter> struct mode_fncall {
        template <typename From> constexpr auto operator()(From&& from) const;
    };

    // Converter Factory.
    // * Creates a custom converter from any lambda or function object.
    // * Usage: (Also refer to the examples below)
    // auto to_hex = cvt::make_custom<mode_pipe>([](u8 v) { ... });
    // auto str    = raw_byte | to_hex;
    template < template <typename> typename... SupportedModes >
    constexpr auto make_custom(auto&& func) { return converter< decltype(func), SupportedModes... >{}; }
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
    template <typename To> inline constexpr auto sc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return static_cast<To>(forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto dc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return dynamic_cast<To>(forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto rc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return reinterpret_cast<To>(forward<decltype(from)>(from)); } );
    template <typename To> inline constexpr auto cc = cvt::make_custom<cvt::mode_fncall, cvt::mode_pipe>( [](auto&& from) constexpr { return const_cast<To>(forward<decltype(from)>(from)); } );
    // You probably want to use this one for most cases, it just wraps to static_cast.
    template <typename To> inline constexpr auto to = sc<To>;

    // The "Expected" Converter.
    // * A specialized "Magic" converter that infers the required destination type
    // from the assignment context / function return type.
    // * Example:
    // u16 value = 500.0f | cvt::toe; // Compiler infers u16
    struct expected;
    inline constexpr auto toe          = cvt::converter< expected, cvt::mode_fncall, cvt::mode_pipe>{};
    inline constexpr auto to_expected  = cvt::converter< expected, cvt::mode_fncall, cvt::mode_pipe>{};
}

// --- Implementation Details ---

namespace lm::cvt
{
    // Function Call Implementation.
    template <typename UnderlyingConverter>
    template <typename From>
    constexpr auto mode_fncall<UnderlyingConverter>::operator()(From&& from) const
    { return UnderlyingConverter{}( forward<From>(from) ); }

    // Arithmetic Multiplicative (*).
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator*(T&& val, const mode_math<UnderlyingConverter>&)
    { return UnderlyingConverter{}( forward<T>(val) ); }
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator*(const mode_math<UnderlyingConverter>&, T&& val)
    { return UnderlyingConverter{}( forward<T>(val) ); }

    // Arithmetic Division (/).
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator/(T&& val, const mode_math<UnderlyingConverter>&)
    { return UnderlyingConverter{}( forward<T>(val) ); }
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator/(const mode_math<UnderlyingConverter>&, T&& val)
    { return UnderlyingConverter{}( forward<T>(val) ); }

    // Arithmetic Additive (+).
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator+(T&& val, const mode_math<UnderlyingConverter>&)
    { return UnderlyingConverter{}( forward<T>(val) ); }
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator+(const mode_math<UnderlyingConverter>&, T&& val)
    { return UnderlyingConverter{}( forward<T>(val) ); }

    // Arithmetic Subtraction (-).
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator-(T&& val, const mode_math<UnderlyingConverter>&)
    { return UnderlyingConverter{}( forward<T>(val) ); }
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator-(const mode_math<UnderlyingConverter>&, T&& val)
    { return UnderlyingConverter{}( forward<T>(val) ); }

    // Pipe Operator Implementation.
    template <typename T, typename UnderlyingConverter>
    constexpr auto operator|(T&& val, const mode_pipe<UnderlyingConverter>&)
    { return UnderlyingConverter{}( forward<T>(val) ); }
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
            { return move(v) | to<ExpectedType>; }
        };

        template <typename From>
        constexpr auto operator()(From&& from) const
        { return implicit_cvt<From>{ forward<From>(from) }; }
    };
}
