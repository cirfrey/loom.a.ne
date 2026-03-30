#pragma once

namespace lm
{
    template <typename T> struct remove_ref      { using type = T; };
    template <typename T> struct remove_ref<T&>  { using type = T; };
    template <typename T> struct remove_ref<T&&> { using type = T; };
    template <typename T> using  remove_ref_t = typename remove_ref<T>::type;

    template <typename T> struct remove_cv                   { using type = T; };
    template <typename T> struct remove_cv<const T>          { using type = T; };
    template <typename T> struct remove_cv<volatile T>       { using type = T; };
    template <typename T> struct remove_cv<const volatile T> { using type = T; };
    template <typename T> using  remove_cv_t = typename remove_cv<T>::type;

    template <typename T> struct remove_cvref { using type = remove_cv_t< remove_ref_t<T> >; };
    template <typename T> using  remove_cvref_t = typename remove_cvref<T>::type;

    template <typename T>
    constexpr auto forward(remove_ref_t<T>& t) noexcept -> T&&
    { return static_cast<T&&>(t); }

    template <typename T>
    constexpr auto forward(remove_ref_t<T>&& t) noexcept -> T&&
    { return static_cast<T&&>(t); }

    template<typename T>
    constexpr auto move(T&& t) noexcept -> remove_ref_t<T>&&
    { return static_cast<remove_ref_t<T>&&>(t); }

    template <bool B> struct bool_constant { static constexpr bool value = B; };
    using true_type  = bool_constant<true>;
    using false_type = bool_constant<false>;

    // --- Type Relationships ---
    template <typename T, typename U> struct is_same : false_type {};
    template <typename T> struct is_same<T, T> : true_type {};
    template <typename T, typename U>
    inline constexpr bool is_same_v = is_same<T, U>::value;

    // --- Integral Identification ---
    // We define this explicitly for your types.hpp aliases
    template <typename T> struct is_integral : false_type {};

    // Macro to avoid repetitive typing for signed/unsigned pairs
    #define LOOMANE_TRAITS_MARK_INTEGRAL(TYPE) \
        template <> struct is_integral<TYPE> : true_type {}; \
        template <> struct is_integral<unsigned TYPE> : true_type {};

    LOOMANE_TRAITS_MARK_INTEGRAL(char)
    LOOMANE_TRAITS_MARK_INTEGRAL(short)
    LOOMANE_TRAITS_MARK_INTEGRAL(int)
    LOOMANE_TRAITS_MARK_INTEGRAL(long)
    LOOMANE_TRAITS_MARK_INTEGRAL(long long)
    template <> struct is_integral<bool> : true_type {};
    #undef LOOMANE_TRAITS_MARK_INTEGRAL

    template <typename T>
    inline constexpr bool is_integral_v = is_integral<remove_cvref_t<T>>::value;

    // --- Signed vs Unsigned ---
    template <typename T> struct is_signed : false_type {};
    template <> struct is_signed<signed char> : true_type {};
    template <> struct is_signed<signed short> : true_type {};
    template <> struct is_signed<signed int> : true_type {};
    template <> struct is_signed<signed long> : true_type {};
    template <> struct is_signed<signed long long> : true_type {};

    template <typename T>
    inline constexpr bool is_signed_v = is_signed<remove_cvref_t<T>>::value;
}
