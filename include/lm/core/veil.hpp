#pragma once

#include <type_traits>
#include <utility>

namespace lm::veil
{
    // --- Forwarding & Moving ---
    using std::forward;
    using std::move;

    // --- Core Constants ---
    using std::bool_constant;
    using std::true_type;
    using std::false_type;
    using std::integral_constant;

    // --- Type Modifications ---
    using std::remove_reference_t;
    using std::remove_reference;
    // Mapping `remove_ref` to standard `remove_reference`.
    template <typename T> using remove_ref   = std::remove_reference<T>;
    template <typename T> using remove_ref_t = std::remove_reference_t<T>;
    using std::remove_cv;
    using std::remove_cv_t;

    using std::remove_extent;
    using std::remove_extent_t;

    using std::remove_cvref;
    using std::remove_cvref_t;

    using std::decay;
    using std::decay_t;

    using std::conditional;
    using std::conditional_t;

    using std::add_pointer;
    using std::add_pointer_t;

    // --- Type Queries ---
    using std::is_same;
    using std::is_same_v;

    using std::is_integral;
    using std::is_integral_v;

    using std::is_signed;
    using std::is_signed_v;

    using std::is_array;
    using std::is_array_v;

    using std::is_class;
    using std::is_class_v;

    using std::is_empty;
    using std::is_empty_v;

    using std::is_function;
    using std::is_function_v;

    using std::extent;
    using std::extent_v;

    template <bool v, typename... Ts>
    struct assert_helper_t {
        static_assert(v, "Assertion failed");
        static constexpr auto value = v;
    };
    template <bool v, typename... Ts>
    constexpr auto assert_helper = assert_helper_t<v, Ts...>::value;
}
