// Maybe it's a smart idea to abstract from freeRTOS... Maybe not, I don't know yet.
#pragma once

#include "espy/aliases.hpp"
#include "espy/bus.hpp"

// Forward decl.
namespace espy::config::task { struct info_t; }

namespace espy::task
{
    // Safeguard so we only create the tasks we know of (requires an espy::config::task::info_t)
    using task_function_t = void (*)(void*);
    auto create(espy::config::task::info_t const& info, task_function_t task, void* params = nullptr) -> void* /* Handle */;

    auto delay_ms(unsigned long ms) -> void;

    struct queue_t;
    template <typename T>
    auto queue(u32 max_elements) -> queue_t;

    // Event bus: wrapper around making a queue + subscribing with for-loop iteration.
    struct event_bus_t;
    auto event_bus(u32 max_events, espy::bus::topic_t topic, std::initializer_list<u8> types = {}) -> event_bus_t;
}

// Impls.

struct espy::task::queue_t
{
    queue_t() = default;
    queue_t(u32 max_elements, u32 element_size_in_bytes);
    queue_t(queue_t const&) = delete;
    queue_t(queue_t&& o);
    auto operator=(const queue_t&) -> queue_t& = delete;
    auto operator=(queue_t&& o) -> queue_t&;

    auto send(void const* item, u32 timeout) -> void;
    auto receive(void* into, u32 timeout) -> bool;

    ~queue_t();

private:
    void* impl = nullptr;
};

template <typename T>
auto espy::task::queue(u32 max_elements) -> queue_t
{ return queue_t(max_elements, sizeof(T)); }

struct espy::task::event_bus_t
{
    event_bus_t() = default;
    event_bus_t(event_bus_t const&) = delete;
    event_bus_t(event_bus_t&& o);
    auto operator=(const event_bus_t&) -> event_bus_t& = delete;
    auto operator=(event_bus_t&& o) -> event_bus_t&;

    struct iterator
    {
        event_bus_t* bus;
        espy::bus::event current_event;
        bool is_done;

        // Iterator requirements for range-based for
        auto operator*() -> espy::bus::event&;
        auto operator++() -> iterator&;
        auto operator!=(const iterator& other) const -> bool;
    };
    auto begin() -> iterator;
    auto end() -> iterator;

    ~event_bus_t() = default;

private:
    friend auto event_bus(u32, espy::bus::topic_t, std::initializer_list<u8>) -> event_bus_t;
    queue_t q = {};
    espy::bus::subscribe_token tok = {};
};
