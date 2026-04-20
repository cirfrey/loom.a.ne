// include/lm/fabric/id_registry.hpp
#pragma once

#include "lm/core/types.hpp"

namespace lm::fabric::strand::registry
{
    auto init() -> void;

    struct reserve_result
    {
        enum status_t { ok, error } status;
        u8 id;

        constexpr auto is_ok()     { return status == ok; }
        constexpr auto has_error() { return status == error; }
    };
    [[nodiscard]] auto reserve(u8 id) -> reserve_result;
    [[nodiscard]] auto reserve()      -> reserve_result;

    auto unreserve(u8 id) -> void;
}
