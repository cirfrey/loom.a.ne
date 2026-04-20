#pragma once

#include "lm/core/types.hpp"
#include <bit>
#include <type_traits>

namespace lm
{

template <st N>
struct bitset
{
    static_assert(N > 0);

    static constexpr st bits        = N;
    static constexpr st bucket_bits = 32;
    static constexpr st num_buckets = (N + bucket_bits - 1) / bucket_bits;
    static constexpr st npos        = ~st{0};

    using bucket_t = u32;

    bucket_t buckets[num_buckets] = {};

    // ── Single-bit access ─────────────────────────────────────────────────

    constexpr auto set(st n) -> bitset&
    {
        buckets[n / bucket_bits] |= (bucket_t{1} << (n % bucket_bits));
        return *this;
    }

    constexpr auto clear(st n) -> bitset&
    {
        buckets[n / bucket_bits] &= ~(bucket_t{1} << (n % bucket_bits));
        return *this;
    }

    constexpr auto flip(st n) -> bitset&
    {
        buckets[n / bucket_bits] ^= (bucket_t{1} << (n % bucket_bits));
        return *this;
    }

    constexpr auto test(st n) const -> bool
    {
        return (buckets[n / bucket_bits] >> (n % bucket_bits)) & 1u;
    }

    constexpr auto operator[](st n) const -> bool { return test(n); }

    // ── Bulk operations ───────────────────────────────────────────────────

    constexpr auto set_all() -> bitset&
    {
        for(auto& b : buckets) b = ~bucket_t{0};
        trim_tail();
        return *this;
    }

    constexpr auto clear_all() -> bitset&
    {
        for(auto& b : buckets) b = 0;
        return *this;
    }

    constexpr auto flip_all() -> bitset&
    {
        for(auto& b : buckets) b = ~b;
        trim_tail();
        return *this;
    }

    // ── Queries ───────────────────────────────────────────────────────────

    constexpr auto any()  const -> bool { for(auto b : buckets) if(b)  return true;  return false; }
    constexpr auto none() const -> bool { return !any(); }

    constexpr auto all() const -> bool
    {
        for(st i = 0; i < num_buckets - 1; ++i)
            if(buckets[i] != ~bucket_t{0}) return false;
        return buckets[num_buckets - 1] == tail_mask();
    }

    // Number of set bits — popcount across all buckets.
    constexpr auto count() const -> st
    {
        st n = 0;
        for(auto b : buckets) n += std::popcount(b);
        return n;
    }

    // Number of clear bits.
    constexpr auto count_clear() const -> st { return N - count(); }

    // ── Search — O(num_buckets) using <bit> ───────────────────────────────
    //
    // countr_zero(x)  → index of lowest set   bit in x, or 32 if x == 0
    // countr_zero(~x) → index of lowest clear bit in x, or 32 if x == ~0
    // countl_zero(x)  → 31 - index of highest set bit in x

    // First SET bit at or after `from`. Returns npos if none.
    constexpr auto find_first_set(st from = 0) const -> st
    {
        if(from >= N) return npos;
        st bi = from / bucket_bits;
        st bo = from % bucket_bits;

        // Mask off bits before `from` in the first bucket
        bucket_t b = buckets[bi] & (~bucket_t{0} << bo);
        if(b) return bi * bucket_bits + std::countr_zero(b);

        for(st i = bi + 1; i < num_buckets; ++i)
            if(buckets[i]) return i * bucket_bits + std::countr_zero(buckets[i]);

        return npos;
    }

    // First CLEAR bit at or after `from`. Returns npos if none.
    constexpr auto find_first_clear(st from = 0) const -> st
    {
        if(from >= N) return npos;
        st bi = from / bucket_bits;
        st bo = from % bucket_bits;

        bucket_t b = ~buckets[bi] & (~bucket_t{0} << bo);
        if(b) {
            st pos = bi * bucket_bits + std::countr_zero(b);
            return pos < N ? pos : npos;
        }

        for(st i = bi + 1; i < num_buckets; ++i) {
            if(~buckets[i]) {
                st pos = i * bucket_bits + std::countr_zero(~buckets[i]);
                return pos < N ? pos : npos;
            }
        }

        return npos;
    }

    // Last SET bit. Returns npos if none.
    constexpr auto find_last_set() const -> st
    {
        for(st i = num_buckets; i-- > 0;)
            if(buckets[i])
                return i * bucket_bits + (bucket_bits - 1 - std::countl_zero(buckets[i]));
        return npos;
    }

    // Last CLEAR bit. Returns npos if none.
    constexpr auto find_last_clear() const -> st
    {
        for(st i = num_buckets; i-- > 0;) {
            bucket_t inv = ~buckets[i];
            if(i == num_buckets - 1) inv &= tail_mask();
            if(inv)
                return i * bucket_bits + (bucket_bits - 1 - std::countl_zero(inv));
        }
        return npos;
    }

    // Count SET bits in [from, to).
    constexpr auto count_range(st from, st to) const -> st
    {
        st n = 0;
        for(st i = from; i < to;) {
            st bi = i / bucket_bits;
            st lo = i % bucket_bits;
            st hi = (bi == to / bucket_bits) ? to % bucket_bits : bucket_bits;
            bucket_t mask = ((hi < bucket_bits) ? (bucket_t{1} << hi) - 1 : ~bucket_t{0})
                          & (~bucket_t{0} << lo);
            n += std::popcount(buckets[bi] & mask);
            i = (bi + 1) * bucket_bits;
        }
        return n;
    }

    // ── Slice read: extract Width bits starting at Start ──────────────────
    //
    // Return type is the smallest unsigned integer that fits Width bits.
    // Fully constexpr — compiler resolves the cross-bucket logic at compile
    // time when Start and Width are compile-time constants.
    //
    // Example:
    //   auto id_range = bs.slice<32, 8>();   // returns u8
    //   auto word     = bs.slice<0, 32>();   // returns u32

    template <st Start, st Width>
    constexpr auto slice() const
    {
        static_assert(Start + Width <= N, "slice out of range");
        static_assert(Width > 0 && Width <= 64, "width must be in [1, 64]");

        using ret_t = std::conditional_t<Width <=  8, u8,
                      std::conditional_t<Width <= 16, u16,
                      std::conditional_t<Width <= 32, u32, u64>>>;

        constexpr st lo_bucket    = Start / bucket_bits;
        constexpr st lo_offset    = Start % bucket_bits;
        constexpr st bits_in_lo   = bucket_bits - lo_offset;

        if constexpr (Width <= bits_in_lo) {
            // Entirely within one bucket — single shift and mask
            if constexpr (Width == bucket_bits)
                return static_cast<ret_t>(buckets[lo_bucket] >> lo_offset);
            else
                return static_cast<ret_t>((buckets[lo_bucket] >> lo_offset)
                                          & ((bucket_t{1} << Width) - 1));
        } else {
            // Spans bucket boundary — assemble from successive buckets
            u64 result      = static_cast<u64>(buckets[lo_bucket]) >> lo_offset;
            st  collected   = bits_in_lo;
            st  bi          = lo_bucket + 1;

            while(collected < Width && bi < num_buckets) {
                st  chunk = Width - collected;
                u64 contribution = static_cast<u64>(buckets[bi]);
                if(chunk < bucket_bits)
                    contribution &= (u64{1} << chunk) - 1;
                result    |= contribution << collected;
                collected += bucket_bits;
                ++bi;
            }
            return static_cast<ret_t>(result);
        }
    }

    // ── Slice write: write Width bits of `value` starting at Start ────────

    template <st Start, st Width, typename T>
    constexpr auto set_slice(T value) -> bitset&
    {
        static_assert(Start + Width <= N, "slice out of range");
        static_assert(Width > 0 && Width <= 64);

        u64 v      = static_cast<u64>(value);
        st  bi     = Start / bucket_bits;
        st  offset = Start % bucket_bits;
        st  written = 0;

        while(written < Width) {
            st will_write = bucket_bits - offset < Width - written
                          ? bucket_bits - offset
                          : Width - written;

            bucket_t mask = will_write == bucket_bits
                          ? ~bucket_t{0}
                          : ((bucket_t{1} << will_write) - 1) << offset;

            bucket_t bits = static_cast<bucket_t>(v & (will_write == 64
                          ? ~u64{0} : (u64{1} << will_write) - 1)) << offset;

            buckets[bi] = (buckets[bi] & ~mask) | bits;
            v       >>= will_write;
            written  += will_write;
            ++bi;
            offset = 0;
        }
        return *this;
    }

    // ── Shift ─────────────────────────────────────────────────────────────

    constexpr auto operator<<=(st n) -> bitset&
    {
        if(n >= N) { clear_all(); return *this; }
        st bucket_shift = n / bucket_bits;
        st bit_shift    = n % bucket_bits;

        for(st i = num_buckets; i-- > 0;) {
            bucket_t lo = i >= bucket_shift     ? buckets[i - bucket_shift] : 0;
            bucket_t hi = i >= bucket_shift + 1 && bit_shift > 0
                        ? buckets[i - bucket_shift - 1] >> (bucket_bits - bit_shift)
                        : 0;
            buckets[i] = (lo << bit_shift) | hi;
        }
        trim_tail();
        return *this;
    }

    constexpr auto operator>>=(st n) -> bitset&
    {
        if(n >= N) { clear_all(); return *this; }
        st bucket_shift = n / bucket_bits;
        st bit_shift    = n % bucket_bits;

        for(st i = 0; i < num_buckets; ++i) {
            bucket_t hi = i + bucket_shift < num_buckets ? buckets[i + bucket_shift] : 0;
            bucket_t lo = i + bucket_shift + 1 < num_buckets && bit_shift > 0
                        ? buckets[i + bucket_shift + 1] << (bucket_bits - bit_shift)
                        : 0;
            buckets[i] = (hi >> bit_shift) | lo;
        }
        return *this;
    }

    friend constexpr auto operator<<(bitset b, st n) -> bitset { return b <<= n; }
    friend constexpr auto operator>>(bitset b, st n) -> bitset { return b >>= n; }

    // ── Bitwise operators ─────────────────────────────────────────────────

    constexpr auto operator&=(bitset const& o) -> bitset&
    { for(st i = 0; i < num_buckets; ++i) buckets[i] &= o.buckets[i]; return *this; }

    constexpr auto operator|=(bitset const& o) -> bitset&
    { for(st i = 0; i < num_buckets; ++i) buckets[i] |= o.buckets[i]; return *this; }

    constexpr auto operator^=(bitset const& o) -> bitset&
    { for(st i = 0; i < num_buckets; ++i) buckets[i] ^= o.buckets[i]; return *this; }

    constexpr auto operator~() const -> bitset { bitset r = *this; r.flip_all(); return r; }

    friend constexpr auto operator&(bitset a, bitset const& b) -> bitset { return a &= b; }
    friend constexpr auto operator|(bitset a, bitset const& b) -> bitset { return a |= b; }
    friend constexpr auto operator^(bitset a, bitset const& b) -> bitset { return a ^= b; }

    constexpr auto operator==(bitset const&) const -> bool = default;

private:
    // Mask for valid bits in the last bucket.
    // If N is a multiple of bucket_bits, all bits are valid → ~0.
    static constexpr auto tail_mask() -> bucket_t
    {
        if constexpr (N % bucket_bits == 0)
            return ~bucket_t{0};
        else
            return (bucket_t{1} << (N % bucket_bits)) - 1;
    }

    // Clear any bits beyond N in the last bucket.
    // Called after any bulk operation that might set them.
    constexpr auto trim_tail() -> void
    {
        if constexpr (N % bucket_bits != 0)
            buckets[num_buckets - 1] &= tail_mask();
    }
};

} // namespace lm
