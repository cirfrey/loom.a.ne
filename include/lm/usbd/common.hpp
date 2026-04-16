// Some stuff so you write less boilerplate when implementing the backends for
// descriptors and such.
#pragma once

#include <span>
#include <cstring>

#include "lm/strands/usbd.hpp"

namespace lm::usbd
{
    using cfg_t = configuration_t;

    using ep_t = endpoint_info_t;
    using itf_t = ep_t::interface_type_t;
    using ept_t = ep_t::type_t;

    struct configuration_descriptor_builder_state_t
    {
        u8* const desc; // Points to the beginning of the descriptor.
        u8 lowest_free_itf_idx;
        u16 desc_curr_len;
        const u16 desc_max_len;

        template <u8 DataSize>
        auto append_desc(u8 const (&data)[DataSize]) -> bool
        {
            if(desc_curr_len + DataSize > desc_max_len) return false;

            std::memcpy(desc + desc_curr_len, data, DataSize);
            desc_curr_len += DataSize;
            return true;
        }
    };

    enum class string_descriptor_idxs : u8
    {
        idx_lang = 0,
        idx_manufacturer = 1,
        idx_product = 2,
        idx_serial = 3,
        idx_midi = 4,
        idx_hid = 5,
        idx_uac = 6,
        idx_cdc = 7,
        idx_msc = 8,
        idx_max
    };

    constexpr auto EP_DIR_IN = 0x80_u8; // When ep direction is IN we need to do EP_DIR_IN | ep_idx in the configuration descriptor.

    struct find_unassigned_ep_ret_t { u8 idx = 0; ep_t* ep = nullptr; };
    inline auto find_unassigned_ep_in(std::span<ep_t> eps)
    {
        using ret_t = find_unassigned_ep_ret_t;

        for(u8 i = 1; i < eps.size(); ++i)
            if(eps[i].in == ept_t::unassigned)
                return ret_t{ .idx = i, .ep = eps.data() + i };
        return ret_t {};
    };
    inline auto find_unassigned_ep_out(std::span<ep_t> eps)
    {
        using ret_t = find_unassigned_ep_ret_t;

        for(u8 i = 1; i < eps.size(); ++i)
            if(eps[i].out == ept_t::unassigned)
                return ret_t{ .idx = i, .ep = eps.data() + i };
        return ret_t {};
    };
    inline auto find_unassigned_ep_inout(std::span<ep_t> eps)
    {
        using ret_t = find_unassigned_ep_ret_t;

        for(u8 i = 1; i < eps.size(); ++i)
            if(eps[i].in == ept_t::unassigned && eps[i].out == ept_t::unassigned)
                return ret_t{ .idx = i, .ep = eps.data() + i };
        return ret_t {};
    };
}
