// Maybe it's a smart idea to abstract from freeRTOS... Maybe not, I don't know yet.
#pragma once

#include "lm/aliases.hpp"
#include "lm/bus.hpp"

#include "lm/task/types.hpp"

namespace lm::task
{
    using task_function_t = void (*)(lm::task::config const&);

    // Safeguard so we only create the tasks we know of (requires an lm::config::task::info_t).
    // A typical task looks like this:
    //
    //     auto lm::whatever::task(lm::task::config const& cfg) -> void
    //     {
    //         using tc = lm::task::event::task_command;
    //
    //         /* Initialize whatever's internal resources here. */
    //
    //         auto tc_bus = tc::make_bus();
    //         tc::wait_for_start(tc_bus, cfg.id);
    //         while(!tc::should_stop(tc_bus, cfg.id))
    //         {
    //             /* Do task processing here */
    //             lm::task::delay_ms(cfg.id);
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
    auto create(lm::task::config const& cfg, task_function_t task, bool do_publish = true) -> void* /* Handle */;

    auto get_handle() -> void*;

    // Deletes a task by it's handle.
    auto reap(void* handle) -> void;

    auto delay_ms(unsigned long ms) -> void;

    struct queue_t;
    template <typename T>
    auto queue(u32 max_elements) -> queue_t;

    // Event bus: wrapper around making a queue + subscribing with for-loop iteration.
    struct event_bus_t;
    auto event_bus(u32 max_events, lm::bus::topic_t topic, std::initializer_list<u8> types = {}) -> event_bus_t;
}

namespace lm::task::event
{
    // From task -> To FSM.
    struct task_status
    {
        enum type : u8
        {
            // Handled by lm::task::create
            created, // event.data = handle
            ready,   // event.data = handle
            running, // event.data = handle
            // When you reveive a stop event, this is what you must ack with signal you
            // are ready for deletion.
            ready_for_shutdown, // event.data = handle
        };

        static auto wait_for_shutdown(lm::task::id_t taskid) -> void;
    };

    // From FSM -> To task.
    struct task_command
    {
        enum type : u8 {
            // Signals that the task can begin processing.
            start, // event.data[0] = taskid
            // Signals that the task should end processing.
            stop,  // event.data[0] = taskid
        };

        static auto make_bus(u32 size = 12) -> event_bus_t;
        // Warning, these discard whatever events you don't care about.
        static auto wait_for_start(event_bus_t&, lm::task::id_t taskid) -> void;
        static auto should_stop(event_bus_t&,    lm::task::id_t taskid) -> bool;
    };
}

// Impls.

struct lm::task::queue_t
{
    queue_t() = default;
    queue_t(u32 max_elements, u32 element_size_in_bytes);
    queue_t(queue_t const&) = delete;
    queue_t(queue_t&& o);
    auto operator=(const queue_t&) -> queue_t& = delete;
    auto operator=(queue_t&& o) -> queue_t&;

    auto send(void const* item, u32 timeout) -> bool;
    auto receive(void* into, u32 timeout) -> bool;

    auto free_spaces_available() const -> u32;
    auto capacity() const -> u32;
    auto element_size() const -> u32;

    ~queue_t();

// private:
    void* impl = nullptr;
    u32 element_size_in_bytes;
    u32 max_elements;
};

template <typename T>
auto lm::task::queue(u32 max_elements) -> queue_t
{ return queue_t(max_elements, sizeof(T)); }

struct lm::task::event_bus_t
{
    event_bus_t() = default;
    event_bus_t(event_bus_t const&) = delete;
    event_bus_t(event_bus_t&& o);
    auto operator=(const event_bus_t&) -> event_bus_t& = delete;
    auto operator=(event_bus_t&& o) -> event_bus_t&;

    struct iterator
    {
        event_bus_t* bus;
        lm::bus::event current_event;
        bool is_done;

        // Iterator requirements for range-based for
        auto operator*() -> lm::bus::event&;
        auto operator++() -> iterator&;
        auto operator!=(const iterator& other) const -> bool;
    };
    auto begin() -> iterator;
    auto end() -> iterator;

    ~event_bus_t() = default;

// private:
    friend auto event_bus(u32, lm::bus::topic_t, std::initializer_list<u8>) -> event_bus_t;
    queue_t q = {};
    lm::bus::subscribe_token tok = {};
};
