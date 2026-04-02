// Maybe it's a smart idea to abstract from freeRTOS... Maybe not, I don't know yet.
#pragma once

#include "lm/core/types.hpp"
#include "lm/core/cvt.hpp"
#include "lm/fabric/primitives.hpp"
#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"

namespace lm::fabric::task
{
    using task_function_t = void (*)(void*);
    using task_handle_t   = void*;

    /** --- A wrapper to make tasking easy ---
     * This is the main way you might interact with the task system.
     * Here's what it expects your task to look like.
     *   struct my_task
     *   {
     *       my_task(lm::fabric::task_runtime_info& info) {}
     *       auto on_ready() {}
     *       auto before_sleep() -> lm::fabric::managed_task_status { return lm::fabric::managed_task_status::ok; }
     *       auto on_wake() -> lm::fabric::managed_task_status { return lm::fabric::managed_task_status::ok; }
     *       ~my_task() {}
     *   }
     */
    template <typename Task>
    constexpr auto managed() -> task_function_t;


    /**  --- Managed task lifetime stuff. ---
     * if you use managed() you won't need this. This is only if you implement raw tasks that interface with
     * fabric::task::create().
     */

    // Assumes the queue is actually subscribed and listening to the signals.
    auto wait_for_start(queue_t&, st taskid) -> void;
    // Assumes the queue is actually subscribed and listening to the signals.
    auto should_stop(queue_t&, st taskid) -> bool;
    auto wait_for_shutdown(st taskid) -> void;

    auto sleep_ms(unsigned long ms) -> void;

    // A typical task looks something like this:
    //
    //     auto lm::whatever::task(lm::task::config const& cfg) -> void
    //     {
    //         using tc = lm::task::event::task_command;
    //
    //         /* Initialize whatever's internal resources here. */
    //
    //         auto tc_bus = tc::make_bus();
    //         tc::wait_for_start(tc_bus, cfg.id);
    //
    //         while(!tc::should_stop(tc_bus, cfg.id))
    //         {
    //             /* Do task processing here */
    //             lm::task::sleep_ms(cfg.sleep_ms);
    //             /* Or here, depends on what makes sense for this task. */
    //         }
    //     }
    //
    // Then you call lm::task::create(lm::config::whatever, lm::whatever::task, (void*)whateverparams).
    // void* whateverparams is forwarded to lm::whatever::task.
    //
    // NOTE: For most cases we want to also publish the task creation event -- EXCEPT for sysman. The problem
    //       is that sysman is responsible for intializing the bus and other things during its boot state and
    //       if we try to publish to the bus before it is initialized nasty things can happen.
    auto create(task_constants const& cfg, task_runtime_info const& info, task_function_t task, bool do_publish = true) -> task_handle_t;
    auto get_handle() -> void*;
    // Deletes a task by it's handle.
    auto reap(void* handle) -> void;

    enum event : u8
    {
        /* --- status updates (payload = status_event) --- */

        created, // Handled by lm::task::create.
        ready,
        running,
        waiting_for_reap,


        /* --- signals (payload = signal_event) --- */

        signal_start,
        signal_stop,
    };

    struct status_event
    {
        void* handle;
    }; static_assert(sizeof(status_event) <= sizeof(fabric::event::payload));

    struct signal_event
    {
        u64 task_id;
    }; static_assert(sizeof(signal_event) <= sizeof(fabric::event::payload));
}

/* --- Impls --- */
template <typename Task>
constexpr auto lm::fabric::task::managed() -> task_function_t
{
    return [](void* param){
        auto info = param | smuggle<task_runtime_info>;

        [&]{
            Task task{info};

            auto signal_queue = fabric::queue<fabric::event>(8);
            auto signal_token = bus::subscribe(
                signal_queue,
                topic::task,
                {event::signal_start, event::signal_stop}
            );
            wait_for_start(signal_queue, info.id);

            if(task.on_ready() == managed_task_status::exit) return;
            while (!should_stop(signal_queue, info.id))
            {
                if(task.before_sleep() == managed_task_status::exit) return;
                if(info.sleep_ms > 0) sleep_ms(info.sleep_ms);
                if(task.on_wake() == managed_task_status::exit) return;
            }
        }();

        wait_for_shutdown(info.id);
    };
}
