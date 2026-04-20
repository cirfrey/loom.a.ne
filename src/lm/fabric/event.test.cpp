#include "lm/test.hpp"
#include "lm/fabric/types.hpp"

LM_TEST_SUITE("fabric.event.layout",
{
    // The 24-byte invariant. If this breaks, the entire bus protocol breaks.
    static_assert(sizeof(lm::fabric::event) == 24,
        "event must be exactly 24 bytes — this is a wire format invariant");
    check(true, "sizeof(event) == 24 (static_assert passed)");

    // Payload capacity
    static_assert(sizeof(lm::fabric::event::payload) == 8,
        "payload must be 8 bytes");
    check(true, "sizeof(payload) == 8 (static_assert passed)");

    // with_payload / get_payload roundtrip
    struct p_t { lm::u32 x; lm::u32 y; };
    static_assert(sizeof(p_t) <= 8, "test payload fits");

    lm::fabric::event e{};
    e.with_payload(p_t{.x = 0xCAFE, .y = 0xBEEF});
    auto& p = e.get_payload<p_t>();
    check(p.x == 0xCAFEu, "payload.x roundtrip");
    check(p.y == 0xBEEFu, "payload.y roundtrip");
});

LM_TEST_SUITE("fabric.event.is_local",
{
    lm::fabric::event local{};
    local.loom_id = 0;
    check(local.is_local(), "loom_id==0 is local");

    lm::fabric::event remote{};
    remote.loom_id = 1;
    check(!remote.is_local(), "loom_id==1 is not local");
});
