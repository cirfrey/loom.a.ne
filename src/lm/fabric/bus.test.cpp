#include "lm/test.hpp"
#include "lm/fabric/bus.hpp"
#include "lm/fabric/primitives.hpp"
#include "lm/fabric/types.hpp"

// ── subscribe / unsubscribe ───────────────────────────────────────────────────

using namespace lm;

LM_TEST_SUITE("bus.subscribe",
{
    // RAII token — subscription dropped when token goes out of scope
    {
        auto q = fabric::queue<fabric::event>(4);
        {
            auto tok = fabric::bus::subscribe(q, fabric::topic::any);
            check(tok.valid(), "token valid after subscribe");
        }
        // Token destroyed — publish should reach zero subscribers
        fabric::event e{};
        e.topic = fabric::topic::any;
        e.type  = 1;
        auto r = fabric::bus::publish(e);
        check(r.published == 0, "no delivery after token destroyed");
    }

    // Topic filtering — wrong topic not delivered
    {
        auto q   = fabric::queue<fabric::event>(4);
        auto tok = fabric::bus::subscribe(q, 0x10);   // subscribe to topic 0x10

        fabric::event e{};
        e.topic = 0x11;   // different topic
        e.type  = 1;
        auto r = fabric::bus::publish(e);
        check(r.published == 0, "wrong topic not delivered");
    }

    // Correct topic delivered
    {
        auto q   = fabric::queue<fabric::event>(4);
        auto tok = fabric::bus::subscribe(q, 0x10);

        fabric::event e{};
        e.topic = 0x10;
        e.type  = 1;
        auto r = fabric::bus::publish(e);
        check(r.published == 1, "correct topic delivered");
    }

    // topic::any receives everything
    {
        auto q   = fabric::queue<fabric::event>(4);
        auto tok = fabric::bus::subscribe(q, fabric::topic::any);

        fabric::event e{};
        e.topic = 0x42;
        e.type  = 7;
        auto r = fabric::bus::publish(e);
        check(r.published == 1, "topic::any receives arbitrary topic");
    }
});

// ── drop counting ─────────────────────────────────────────────────────────────

LM_TEST_SUITE("bus.drop",
{
    // Full queue increments dropped counter, does not block
    {
        auto q   = fabric::queue<fabric::event>(2);   // tiny queue
        auto tok = fabric::bus::subscribe(q, fabric::topic::any);

        fabric::event e{};
        e.topic = fabric::topic::any;
        e.type  = 1;

        fabric::bus::publish(e);  // slot 0
        fabric::bus::publish(e);  // slot 1 — queue now full
        auto r = fabric::bus::publish(e);  // should drop

        check(r.dropped == 1, "drop counted on full queue");
        check(r.published == 0, "dropped event not counted as published");
    }
});

// ── payload roundtrip ─────────────────────────────────────────────────────────

LM_TEST_SUITE("bus.payload",
{
    struct test_payload {
        u16 a;
        u16 b;
        u32 c;
    };
    static_assert(sizeof(test_payload) <= 8, "payload exceeds event capacity");

    auto q   = fabric::queue<fabric::event>(4);
    auto tok = fabric::bus::subscribe(q, fabric::topic::any);

    fabric::event sent{};
    sent.topic = fabric::topic::any;
    sent.type  = 0xAB;
    sent.with_payload(test_payload{.a=0x1234, .b=0x5678, .c=0xDEADBEEF});
    fabric::bus::publish(sent);

    fabric::event recv{};
    bool got = q.receive(&recv, 0);
    check(got, "event received from queue");
    check(recv.type == 0xAB, "type preserved through bus");

    auto& p = recv.get_payload<test_payload>();
    check(p.a == 0x1234,       "payload.a preserved");
    check(p.b == 0x5678,       "payload.b preserved");
    check(p.c == 0xDEADBEEF,   "payload.c preserved");
});
