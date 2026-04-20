// Who likes long/non-descriptive types?
#pragma once

namespace lm
{
    namespace detail
    {
        template <auto...>
        struct false_type { static constexpr bool value = false; };

        template<int Size>
        consteval auto unsigned_integer_sized()
        {
            constexpr auto size = Size/8;
            if      constexpr(sizeof(unsigned char)      == size) { return static_cast<unsigned char>(0); }
            else if constexpr(sizeof(unsigned short)     == size) { return static_cast<unsigned short>(0); }
            else if constexpr(sizeof(unsigned int)       == size) { return static_cast<unsigned int>(0); }
            else if constexpr(sizeof(unsigned long)      == size) { return static_cast<unsigned long>(0); }
            else if constexpr(sizeof(unsigned long long) == size) { return static_cast<unsigned long long>(0); }
            else { static_assert(false_type<Size>::value, "No type of this size"); }
        }

        template<int Size>
        consteval auto signed_integer_sized()
        {
            constexpr auto size = Size/8;
            if      constexpr(sizeof(signed char)      == size) { return static_cast<signed char>(0); }
            else if constexpr(sizeof(signed short)     == size) { return static_cast<signed short>(0); }
            else if constexpr(sizeof(signed int)       == size) { return static_cast<signed int>(0); }
            else if constexpr(sizeof(signed long)      == size) { return static_cast<signed long>(0); }
            else if constexpr(sizeof(signed long long) == size) { return static_cast<signed long long>(0); }
            else { static_assert(false_type<Size>::value, "No type of this size"); }
        }
    };

    using u8  = decltype(detail::unsigned_integer_sized<8>());
    using u16 = decltype(detail::unsigned_integer_sized<16>());
    using u32 = decltype(detail::unsigned_integer_sized<32>());
    using u64 = decltype(detail::unsigned_integer_sized<64>());

    using i8  = decltype(detail::signed_integer_sized<8>());
    using i16 = decltype(detail::signed_integer_sized<16>());
    using i32 = decltype(detail::signed_integer_sized<32>());
    using i64 = decltype(detail::signed_integer_sized<64>());

    // Our very own free-range std::size_t.
    using st  = decltype(sizeof(0));
    // Corresponds to std::intptr_t.
    using ipt = decltype(static_cast<char*>(nullptr) - static_cast<char*>(nullptr));
    // Corresponds to std::uintptr_t.
    using upt = decltype(detail::unsigned_integer_sized<sizeof(ipt) * 8>());

    // Yuck.
    using f32 = float;
    // Doubly yucky.
    using f64 = double;
    static_assert(sizeof(f32) == 32/8 && sizeof(f64) == 64/8);

    inline namespace literals
    {
        constexpr auto operator""_u8(unsigned long long val)  { return static_cast<u8>(val); }
        constexpr auto operator""_u16(unsigned long long val) { return static_cast<u16>(val); }
        constexpr auto operator""_u32(unsigned long long val) { return static_cast<u32>(val); }
        constexpr auto operator""_u64(unsigned long long val) { return static_cast<u64>(val); }

        constexpr auto operator""_i8(unsigned long long val)  { return static_cast<i8>(val); }
        constexpr auto operator""_i16(unsigned long long val) { return static_cast<i16>(val); }
        constexpr auto operator""_i32(unsigned long long val) { return static_cast<i32>(val); }
        constexpr auto operator""_i64(unsigned long long val) { return static_cast<i64>(val); }

        constexpr auto operator""_st(unsigned long long val) { return static_cast<st>(val); }
        constexpr auto operator""_ipt(unsigned long long val) { return static_cast<ipt>(val); }
        constexpr auto operator""_upt(unsigned long long val) { return static_cast<upt>(val); }
    };

    // Non-owning const view over raw binary data.
    struct buf {
        void const* data = nullptr;
        st size          = 0;
    };
    // Non-owning, const view of a character string. NOT null-terminated (.size is law).
    struct text {
        char const* data = nullptr;
        st size          = 0;
        constexpr operator buf() const { return { .data = data, .size = size }; }
        constexpr auto operator==(const char* b) const -> bool
        {
            st i = 0;
            while (b[i] != '\0' && i < size && data[i] == b[i]) ++i;
            return size == i && b[i] == '\0';
        }
        constexpr auto operator==(text const& b) const -> bool
        {
            st i = 0;
            while (i <= b.size && i < size && data[i] == b.data[i]) ++i;
            return size == i && b.size == i;
        }
    };
    // Non-owning, mutable view of a character string, NOT null-terminated (.size is law).
    struct mut_buf {
        void* data = nullptr;
        st size    = 0;
    };
    // Non-owning, mutable view of a character string, NOT null-terminated (.size is law).
    struct mut_text {
        char* data = nullptr;
        st size    = 0;
        constexpr operator mut_buf() { return { .data = data, .size = size }; }
    };

    inline namespace literals
    {
        constexpr auto operator ""_text(char const* str, st len) { return text({str, len}); }
    }

}
