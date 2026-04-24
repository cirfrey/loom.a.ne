#include "lm/registry.hpp"

namespace lm::registry
{
    // We start it with the id 0 reserved since that id has a special meaning (means: unidentified strand)
    // and we don't want to accidentally assign a strand that id.
    u8_registry strand_id = u8_registry({0});
}
