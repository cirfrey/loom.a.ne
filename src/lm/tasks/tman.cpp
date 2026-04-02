#include "lm/tasks/sysman.hpp"

#include "lm/core/types.hpp"
#include "lm/fabric/task.hpp"
#include "lm/fabric/bus.hpp"

#include "lm/config.hpp"

#include "lm/tasks/blink.hpp"
#include "lm/tasks/logging.hpp"
#include "lm/tasks/healthmon.hpp"

#include "lm/core/reflect.hpp"
#include "lm/core/cvt.hpp"

#include <array>

namespace lm::sysman
{
    enum class task_status
    {
        not_created,
        requested_creation,
        created,
        ready,
        running,
        stopped,
        deleted,
    };

    struct task_info_t
    {
        void* handle       = nullptr;
        task_status status = task_status::not_created;
    };

    enum class system_state_t : u8 {
        undefined,
        init_diag,
        booting,
        online,
        portal,
        fault
    };

    struct sysman_t
    {
    public:
        sysman_t(lm::task::config const& cfg); // Initializes the registry and kicks off mandatory tasks
        auto do_loop() -> bool;
        ~sysman_t();

    private:
        auto init_diag() -> void;
        auto booting() -> void;
        auto online() -> void;
        auto portal() -> void;
        auto fault() -> void;

        auto process_events() -> void;
        auto reap_stopped_tasks() -> void;

        // Helpers.
        auto transition(system_state_t to)
        {
            auto _from    = renum<system_state_t>::semi_qualified(state);
            auto _to      = renum<system_state_t>::semi_qualified(to);
            LOOM_TRACE("> [%.*s] => [%.*s]\n", _from.size, _from.data, _to.size, _to.data);
            state = to;
        }

        auto handle(lm::task::id_t id) -> void*&       { return task_registry[(u8)id].handle; }
        auto status(lm::task::id_t id) -> task_status& { return task_registry[(u8)id].status; }

        auto task_transition_from(
            lm::task::id_t id,
            task_status from,
            auto transition,
            task_status to
        ) -> void
        {
            if(task_registry[(u8)id].status != from) return;

            auto _from = renum<task_status>::unqualified(from);
            auto _to   = renum<task_status>::unqualified(to);
            LOOM_TRACE(
                "[%s]: [%.*s] =f=> [%.*s]\n",
                lm::config::task::by_id[(u8)id].name,
                _from.size, _from.data,
                _to.size, _to.data
            );
            transition();
            status(id) = to;
        }

        auto start_task_if_ready(lm::task::id_t id) -> void
        {
            if(status(id) != task_status::ready) return;

            lm::bus::publish({
                .data      = id | to<upt> | rc<void*>,
                .sender_id = lm::task::id_t::sysman,
                .topic     = lm::bus::task_command,
                .type      = lm::task::event::task_command::start,
            });
        }

        lm::task::config const& cfg;
        system_state_t state = system_state_t::undefined;
        std::array<task_info_t, (u8)lm::task::id_t::taskid_count> task_registry;
        lm::task::event_bus_t status_bus;
    };
}

auto lm::sysman::init() -> void { lm::task::create(lm::config::task::sysman, lm::sysman::task, false); }
auto lm::sysman::task(lm::task::config const& cfg) -> void
{
    sysman_t sysman(cfg);
    while (sysman.do_loop());
}

lm::sysman::sysman_t::sysman_t(lm::task::config const& cfg) : cfg{cfg}
{
    LOOM_TRACE("Initializing bus...");
    lm::bus::init();
    LOOM_RTRACE("OK\n");

    // Let's get our handle just for completeness sake.
    handle(cfg.id) = lm::task::get_handle();
    // Yes, I'm pretty certain sysman is running at this point.
    status(cfg.id) = task_status::running;

    status_bus = lm::task::event_bus(16, lm::bus::task_status);
    transition(system_state_t::init_diag);
}

auto lm::sysman::sysman_t::do_loop(void) -> bool
{
    lm::task::sleep_ms(cfg.sleep_ms);

    process_events();
    reap_stopped_tasks();

    switch(state) {
        case system_state_t::init_diag: init_diag(); break;
        case system_state_t::booting:   booting(); break;
        case system_state_t::online:    online(); break;
        case system_state_t::portal:    portal(); break;
        case system_state_t::fault:     fault(); break;
    }

    return true;
}

lm::sysman::sysman_t::~sysman_t() { lm::task::reap(nullptr); }

auto lm::sysman::sysman_t::init_diag() -> void
{
    using id = lm::task::id_t;
    using s = task_status;

    task_transition_from(
        id::blink,
        s::not_created,
        lm::blink::init,
        s::requested_creation
    );
    start_task_if_ready(id::blink);

    task_transition_from(
        id::logging,
        s::not_created,
        lm::logging::init,
        s::requested_creation
    );
    start_task_if_ready(id::logging);

    if(status(id::blink) == s::running && status(id::logging) == s::running) {
        transition(system_state_t::booting);
    }
}

auto lm::sysman::sysman_t::booting() -> void
{
    using id = lm::task::id_t;
    using s = task_status;

    task_transition_from(
        id::healthmon,
        s::not_created,
        lm::healthmon::init,
        s::requested_creation
    );
    start_task_if_ready(id::healthmon);

    // TODO: init other subsystems.
}

auto lm::sysman::sysman_t::online() -> void {}
auto lm::sysman::sysman_t::portal() -> void {}
auto lm::sysman::sysman_t::fault()  -> void {}

auto lm::sysman::sysman_t::process_events() -> void
{
    using tse = lm::task::event::task_status;

    for (auto const& e : status_bus) {
        auto id = (u8)e.sender_id;
        if(id >= task_registry.size()) continue;

        auto& info = task_registry[id];

        // Update handle if provided and we don't know it yet.
        if (info.handle == nullptr && e.data) info.handle = e.data;

        auto new_status = task_status::not_created;
        switch ((tse::type)e.type) {
            case tse::created:            new_status = task_status::created; break;
            case tse::ready:              new_status = task_status::ready;   break;
            case tse::running:            new_status = task_status::running; break;
            case tse::ready_for_shutdown: new_status = task_status::stopped; break;
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
        if(new_status <= info.status) continue;

        auto prev = renum<task_status>::unqualified(info.status);
        auto curr = renum<task_status>::unqualified(new_status);
        LOOM_TRACE(
            "[%s]: [%.*s] => [%.*s]\n",
            lm::config::task::by_id[id].name,
            prev.size, prev.data,
            curr.size, curr.data
        );
        info.status = new_status;
    }
}

auto lm::sysman::sysman_t::reap_stopped_tasks() -> void
{
    for (auto i = (u8)lm::task::id_t::taskid_min; i < task_registry.size(); ++i) {
        auto& info = task_registry[i];
        if(info.status != task_status::stopped) continue;
        if(info.handle == nullptr)
        {
            LOOM_TRACE("I really want to reap task [%s] but can't because I don't know its handle.\n", lm::config::task::by_id[i].name);
            continue;
        }

        LOOM_TRACE("Task [%s] is stopped, will try to reap it.\n", lm::config::task::by_id[i].name);
        fabric::reap(info.handle);
        info.handle = nullptr;
        info.status = task_status::deleted;
        LOOM_TRACE("Task [%s] reaped and memory freed.\n", lm::config::task::by_id[i].name);
    }
}
