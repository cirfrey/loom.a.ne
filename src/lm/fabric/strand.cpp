#include "lm/fabric/strand.hpp"

#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"
#include "lm/core/cvt.hpp"
#include "lm/chip/time.hpp"

#include "lm/log.hpp"

auto lm::fabric::strand::manage(void* _params, type_erased_strand_t strand) -> void
{
    auto params = _params | unsmuggle<managed_strand_params>;

    strand_runtime_info info {
        .created_timestamp = chip::time::uptime(),
        .id = params.id,
        .requested_sleep_ms = params.sleep_ms,
    };

    auto signal_queue = fabric::queue<fabric::event>(8);
    auto signal_token = bus::subscribe(signal_queue, topic::framework, {topic::framework_t::strand_signal::type});

    [&]{
        strand.construct(info);

        wait_for_running(signal_queue, info.id);
        info.running_timestamp = chip::time::uptime();

        if(strand.on_ready() == managed_strand_status::suicidal) return;
        while (!should_stop(signal_queue, info.id))
        {
            auto before_sleep_timestamp = chip::time::uptime();
            if(strand.before_sleep() == managed_strand_status::suicidal) return;
            info.last_before_sleep_delta = chip::time::uptime() - before_sleep_timestamp;

            bus::publish(fabric::event{
                .topic     = topic::framework,
                .type      = topic::framework_t::strand_loop_timing::type,
                .strand_id = info.id,
            }.with_payload(topic::framework_t::strand_loop_timing{ .time = info.last_before_sleep_delta + info.last_on_wake_delta }));

            auto sleep_ms_timestamp = chip::time::uptime();
            sleep_ms(info.requested_sleep_ms);
            info.actual_sleep_delta = chip::time::uptime() - sleep_ms_timestamp;

            auto on_wake_timestamp = chip::time::uptime();
            if(strand.on_wake() == managed_strand_status::suicidal) return;
            info.last_on_wake_delta = chip::time::uptime() - on_wake_timestamp;
        }
    }();

    wait_for_shutdown(signal_queue, info.id);
}

auto lm::fabric::strand::wait_for_running(queue_t& q, u8 strand_id) -> void
{
    using status = topic::framework_t::strand_status;
    using signal = topic::framework_t::strand_signal;

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
