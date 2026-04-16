#include "lm/strands/busmon.hpp"

#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"
#include "lm/log.hpp"

#include <cstdio>

lm::strands::busmon::busmon(fabric::strand_runtime_info& info)
{
    ev_q        = fabric::queue<fabric::event>(128);
    ev_q_tok    = fabric::bus::subscribe(ev_q, fabric::topic::any);
    teach_q     = fabric::queue<fabric::event>(64);
    teach_q_tok = fabric::bus::subscribe(teach_q, fabric::topic::busmon_teach);
}

auto lm::strands::busmon::on_ready() -> fabric::managed_strand_status
{
    // Let's teach ourselves the fallback printer and our own event printer
    // and we'll even be cheeky and do it via the event bus instead of
    // assigning it directly.

    fabric::bus::publish( fabric::event{
        .topic = fabric::topic::busmon_teach,
        .type  = fabric::topic::any,
    }.with_payload( teach_topic{ .stringify = [](auto e, auto buf) -> st {
        return std::snprintf(
            buf.data, buf.size, "unkown_event{ .topic=%u, .type=%u, .payload=%p }",
            e.topic, e.type, e.payload
        );
    }}));

    fabric::bus::publish( fabric::event{
        .topic = fabric::topic::busmon_teach,
        .type  = fabric::topic::busmon_teach,
    }.with_payload( teach_topic{ .stringify = [](auto e, auto buf) -> st {
        return std::snprintf(
            buf.data, buf.size, "busmon::teach_topic{ .topic=%u }",
            e.type
        );
    }}));

    return fabric::managed_strand_status::ok;
}

auto lm::strands::busmon::before_sleep() -> fabric::managed_strand_status
{
    // Learn how to print events of a specific topic.
    for(auto const& e : teach_q.consume<fabric::event>())
        topic_cbs[e.type] = e.get_payload<teach_topic>().stringify;

    // Print all incoming events.
    for(auto const& e : ev_q.consume<fabric::event>())
    {
        // Fallback to default if it is not registered.
        auto cb = topic_cbs[e.topic] != nullptr
            ? topic_cbs[e.topic]
            : topic_cbs[0];

        constexpr auto bufsize = 128;
        char buf[bufsize];
        auto strsize = topic_cbs[e.topic](e, {buf, bufsize});
        lm::log::debug("[BUSMON] %.*s\n", strsize, buf);
    }

    return fabric::managed_strand_status::ok;
}

auto lm::strands::busmon::on_wake() -> fabric::managed_strand_status
{ return fabric::managed_strand_status::ok; }
