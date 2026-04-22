// include/lm/fabric/id_registry.hpp
#pragma once

#include "lm/core/primitives/synchronized_registry.hpp"
#include "lm/core/math.hpp"

namespace lm::registry
{
    using result      = lm::synchronized_registry_reserve_result;
    using u8_registry = synchronized_registry< unsigned_max<8> + 1 >;

    extern u8_registry strand_id;
}
