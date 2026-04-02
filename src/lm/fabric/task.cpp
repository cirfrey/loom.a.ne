#include "lm/fabric/task.hpp"

#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"

#include "lm/core/cvt.hpp"

auto lm::fabric::task::wait_for_start(queue_t& q, st task_id) -> void
{
    auto handle = get_handle();
    bool ready_to_start = false;
    while(!ready_to_start) {
        bus::publish(fabric::event{
            .topic     = topic::task,
            .type      = event::ready,
            .sender_id = task_id | toe,
        }.with_payload<status_event>({ .handle = handle }));

        for(auto const& e : q.consume<fabric::event>()) {
            if(e.type == event::signal_start && e.get_payload<signal_event>().task_id == task_id) {
                ready_to_start = true;
                break;
            }
        }
        if(!ready_to_start) sleep_ms(10);
    }
}


auto lm::fabric::task::should_stop(queue_t& q, st task_id) -> bool
{
    for(auto const& e : q.consume<fabric::event>())
        if(e.type == event::signal_stop && e.get_payload<signal_event>().task_id == task_id)
            return true;

    bus::publish(fabric::event{
        .topic     = topic::task,
        .type      = event::running,
        .sender_id = task_id | toe,
    }.with_payload<status_event>({ .handle = get_handle() }));
    return false;
}

auto lm::fabric::task::wait_for_shutdown(st task_id) -> void
{
    while(1) {
        bus::publish(fabric::event{
            .topic     = topic::task,
            .type      = event::waiting_for_reap,
            .sender_id = task_id | toe,
        }.with_payload<status_event>({ .handle = get_handle() }));
        sleep_ms(1000);
    }
}
