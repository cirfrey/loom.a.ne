#include "lm/fabric/strand_registry.hpp"

#include "lm/core/primitives/bitset.hpp"
#include "lm/fabric/primitives.hpp"


namespace lm::fabric::strand::registry
{
    static lm::bitset<256> ids{};
    semaphore mtx;

    auto init() -> void
    {
        mtx = semaphore::create_binary();
        mtx.give();
    }

    auto reserve(u8 id) -> reserve_result
    {
        mtx.take();
        bool taken = ids.test(id);
        if(!taken) ids.set(id);
        mtx.give();
        return { .status = taken ? reserve_result::error : reserve_result::ok, .id = id };
    }

    auto reserve() -> reserve_result
    {
        mtx.take();
        auto pos = ids.find_first_clear(0);
        if(pos != lm::bitset<256>::npos && pos < 255)
            ids.set(pos);
        mtx.give();

        if(pos != lm::bitset<256>::npos && pos < 255) {
            return { .status = reserve_result::ok, .id = (u8)pos };
        } else {
            return { .status = reserve_result::error, .id = 0 };
        }
    }

    auto unreserve(u8 id) -> void
    {
        mtx.take();
        ids.clear(id);
        mtx.give();
    }
}
