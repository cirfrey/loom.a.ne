#include "lm/tasks/tman.hpp"

#include "lm/core.hpp"
#include "lm/fabric.hpp"

#include "lm/log.hpp"

lm::tasks::tman::tman()
{
    status_q = fabric::queue<fabric::event>(16);
    status_q_tok = fabric::bus::subscribe(status_q, fabric::topic::task, {
        fabric::task::event::created,
        fabric::task::event::ready,
        fabric::task::event::running,
        fabric::task::event::waiting_for_reap,
    });
}

auto lm::tasks::tman::do_loop(st max_tasks, std::span<task_info_t>& tasks) -> bool
{
    fabric::task::sleep_ms(tasks[0].runtime.sleep_ms);

    process_events(tasks);
    spawn_tasks(tasks);
    start_tasks(tasks);

    return true;
}

auto lm::tasks::tman::process_events(std::span<task_info_t>& tasks) -> void
{
    for (auto const& e : status_q.consume<fabric::event>()) {
        task_info_t* info = nullptr;
        for(auto& t : tasks) {
            if(t.runtime.id != e.sender_id) continue;

            info = &t;
            break;
        }
        if(!info) return;

        // Update handle if provided and we don't know it yet.
        auto new_handle = e.get_payload<fabric::task::status_event>().handle;
        if(info->handle == nullptr && new_handle) info->handle = new_handle;

        using ts = task_info_t::task_status_t;
        using tse = fabric::task::event;
        auto new_status = ts::not_created;
        switch ((tse)e.type) {
            case tse::created:          new_status = ts::created; break;
            case tse::ready:            new_status = ts::ready;   break;
            case tse::running:          new_status = ts::running; break;
            case tse::waiting_for_reap: new_status = ts::stopped; break;
        }

        // We can't really go backwards in status unless the task was deleted and we want
        // to recreate it. If we got a lesser status than we already have, that just means
        // we received a stale message or there was some sync/bus contention issues (they
        // sent their message with their old status before they got ours or something like that).
        /// TODO: handle exactly that case.
        //
        /// TODO: we *could* solve it by adding a timestamp to the event and keeping track
        ///       of *when* the state changed, ignoring messages older than the last state
        ///       change, but that just seems like a lot of effort.
        if(new_status <= info->status) continue;

        auto prev = renum<ts>::unqualified(info->status);
        auto curr = renum<ts>::unqualified(new_status);
        log::info(
            "[%s]: [%.*s] => [%.*s]\n",
            info->constants.name,
            prev.size, prev.data,
            curr.size, curr.data
        );
        info->status = new_status;
    }
}

auto lm::tasks::tman::spawn_tasks(std::span<task_info_t>& tasks) -> void
{
    // TODO: very simple logic for now, need to implement the dependencies logic.
    for(auto i = 1; i < tasks.size(); ++i)
    {
        if(!(tasks[i].status == task_info_t::task_status_t::not_created && tasks[i].should_be_running)) continue;

        // TODO: handle task.code == null;
        tasks[i].status = task_info_t::task_status_t::requested_creation;
        auto h = fabric::task::create(tasks[i].constants, tasks[i].runtime, tasks[i].code);
        tasks[i].handle = h;
    }
}

auto lm::tasks::tman::start_tasks(std::span<task_info_t>& tasks) -> void
{
    for(auto i = 1; i < tasks.size(); ++i)
    {
        if(!(tasks[i].status == task_info_t::task_status_t::ready && tasks[i].should_be_running)) continue;

        fabric::bus::publish(fabric::event{
            .topic = fabric::topic::task,
            .type  = fabric::task::event::signal_start,
            .sender_id = tasks[0].runtime.id,
        }.with_payload(fabric::task::signal_event{ .task_id = tasks[i].runtime.id }));
    }
}
