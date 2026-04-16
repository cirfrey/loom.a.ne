#pragma once

#include "lm/core/types.hpp"
#include "lm/usb/common.hpp"

#include <span>

namespace lm::usb::debug
{
    /**
     * Table Layout Constants (Documentation Only):
     * - Row Width: 84 chars + '\n' = 85 bytes
     * - Static Rows: 3 separators + 1 header = 4 rows
     * - Data Rows: 85 * ArrSize
     * - Null Terminator: 1 byte
     */
    constexpr auto ept_row_width = 85;
    template <u32 ArrSize>
    constexpr st ep_table_size = (ept_row_width * 4) + (ept_row_width * ArrSize) + 1;

    inline auto print_ep_table(std::span<ep_t> eps, auto printer, text prefix = {nullptr, 0}) -> void;
}


/// Impls.

#include "lm/core/reflect.hpp"
#include "lm/core/math.hpp"

inline auto lm::usb::debug::print_ep_table(std::span<ep_t> eps, auto printer, text prefix) -> void
{
    const char* sep = "+----+--------------------------------------+--------------------------------------+\n";

    printer("%.*s%s", (int)prefix.size, prefix.data, sep);
    printer(
        "%.*s| EP | (itf id:itf type ) IN                | (itf id:itf type ) OUT               |\n",
        (int)prefix.size, prefix.data
    );
    printer("%.*s%s", (int)prefix.size, prefix.data, sep);

    for (size_t i = 0; i < eps.size(); ++i) {
        const auto& ep = eps[i];

        using re_ept = renum<ept_t, 0, 20>;
        using re_itf = renum<itf_t, 0, 10>;
        auto in      = re_ept::unqualified(ep.in);
        auto in_itf  = re_itf::unqualified(ep.in_itf);
        auto out     = re_ept::unqualified(ep.out);
        auto out_itf = re_itf::unqualified(ep.out_itf);

        printer(
            "%.*s|  %zu | (%u:%-14.*s) %-17.*s | (%u:%-14.*s) %-17.*s |\n",
            (int)prefix.size, prefix.data,
            i,
            ep.in_itf_idx,  (int)in_itf.size,  in_itf.data,  (int)in.size, in.data,
            ep.out_itf_idx, (int)out_itf.size, out_itf.data, (int)out.size, out.data
        );
    }

    printer("%.*s%s", (int)prefix.size, prefix.data, sep);
}
