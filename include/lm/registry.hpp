// include/lm/fabric/id_registry.hpp
#pragma once

#include "lm/core/primitives/synchronized_registry.hpp"
#include "lm/core/math.hpp"

namespace lm::registry
{
    using result = lm::synchronized_registry_reserve_result;

    // We start it with the id 0 reserved since that id has a special meaning (means: any strand)
    // and we don't want to accidentally assign a strand that id.
    inline static auto strand_id = synchronized_registry< unsigned_max<8> >({0});
}
