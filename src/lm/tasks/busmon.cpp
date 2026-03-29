#include "lm/tasks/busmon.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"
#include "lm/tasks/logging.hpp"
#include "lm/utils/renum.hpp"

#include <cstdio>

// Default fallback for when we don't know the event/topic.
LOOM_BUSMON_PRINTER(fallback_printer, {
    auto topic_sv = lm::renum::reflect<lm::bus::topic_t>::semi_qualified(e.topic);
    return std::snprintf(buf, bufsize, "unkown_event{ .topic=%.*s, .type=%u, .data=%p }", topic_sv.size(), topic_sv.data(), e.type, e.data);
});
LOOM_BUSMON_PRINTER(busmon_printer, {
    auto topic_sv = lm::renum::reflect<lm::bus::topic_t>::semi_qualified((lm::bus::topic_t)e.type);
    return std::snprintf(buf, bufsize, "busmon::teach_topic{ .topic=%.*s }", topic_sv.size(), topic_sv.data());
});

auto lm::busmon::init() -> void { lm::task::create(lm::config::task::busmon, lm::busmon::task); }
auto lm::busmon::task(lm::task::config const& cfg) -> void
{
    using tc = lm::task::event::task_command;

    auto ev_bus     = lm::task::event_bus(128, lm::bus::any);
    auto busmon_bus = lm::task::event_bus(64, lm::bus::busmon);

    lm::busmon::to_string_cb_t topic_cbs[64] = {nullptr};
    // Let's teach ourselves the fallback printer and our own event printer and we'll even be
    // cheeky and do it via the event bus instead of assigning it directly.
    lm::bus::publish({ .data = &busmon_printer,   .topic = lm::bus::busmon, .type = lm::bus::busmon });
    lm::bus::publish({ .data = &fallback_printer, .topic = lm::bus::busmon, .type = lm::bus::any });

    auto tc_bus = tc::make_bus();
    tc::wait_for_start(tc_bus, cfg.id);
    while(!tc::should_stop(tc_bus, cfg.id))
    {
        // Learn how to print events of a specific topic (see busmon.hpp for busmon::event format).
        for(auto const& ev : busmon_bus) topic_cbs[ev.type] = (lm::busmon::to_string_cb_t)ev.data;

        // Print all incoming events.
        for(auto const& ev : ev_bus)
        {
            auto cb = topic_cbs[ev.topic] != nullptr
                ? topic_cbs[ev.topic]
                : topic_cbs[0];

            constexpr auto bufsize = 128;
            char buf[bufsize];
            auto strsize = topic_cbs[ev.topic](ev, buf, bufsize);
            lm::log::logf("[BUSMON] %.*s\n", strsize, buf);
        }
        lm::task::delay_ms(cfg.sleep_ms);
    }
}
