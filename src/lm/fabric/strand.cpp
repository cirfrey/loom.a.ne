#include "lm/fabric/strand.hpp"

#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"

#include "lm/core/cvt.hpp"

auto lm::fabric::strand::wait_for_running(queue_t& q, u8 strand_id) -> void
{
    using status = topic::framework_t::strand_status;
    using signal = topic::framework_t::strand_signal;

    auto handle = get_handle();

    while(true) {
        bus::publish(fabric::event{
            .topic     = topic::framework,
            .type      = status::type,
            .strand_id = strand_id | toe,
        }.with_payload<status>({ .status = status::ready }));

        for(auto const& e : q.consume<fabric::event>()) {
            if(e.type != signal::type) continue;

            auto data = e.get_payload<signal>();
            if(data.target_strand == strand_id && data.signal == signal::start) return;
        }

        sleep_ms(10);
    }
}

auto lm::fabric::strand::should_stop(queue_t& q, u8 strand_id) -> bool
{
    using status = topic::framework_t::strand_status;
    using signal = topic::framework_t::strand_signal;

    for(auto const& e : q.consume<fabric::event>())
    {
        if(e.type != signal::type) continue;

        auto data = e.get_payload<signal>();
        if(data.target_strand == strand_id && data.signal == signal::stop) return true;
    }

    bus::publish(fabric::event{
        .topic     = topic::framework,
        .type      = status::type,
        .strand_id = strand_id | toe,
    }.with_payload<status>({ .status = status::running }));
    return false;
}

auto lm::fabric::strand::wait_for_shutdown(queue_t& q, u8 strand_id) -> void
{
    using status = topic::framework_t::strand_status;
    using signal = topic::framework_t::strand_signal;

    while(1)
    {
        for(auto const& e : q.consume<fabric::event>())
        {
            if(e.type != signal::type) continue;

            auto data = e.get_payload<signal>();
            if(data.target_strand == strand_id && data.signal == signal::die) return;
        }

        bus::publish(fabric::event{
            .topic     = topic::framework,
            .type      = status::type,
            .strand_id = strand_id | toe,
        }.with_payload<status>({ .status = status::stopped }));

        sleep_ms(1000);
    }
}
