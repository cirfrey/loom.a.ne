#pragma once

#include "lm/core/types.hpp"

namespace lm::fabric
{
    struct queue_t;
    template <typename T>
    auto queue(st max_elements) -> queue_t;

    struct bytebuf;

    struct semaphore;

}

struct lm::fabric::bytebuf
{
    bytebuf() = default;
    bytebuf(st bytes);
    bytebuf(bytebuf const&) = delete;
    bytebuf(bytebuf&& o);
    auto operator=(const bytebuf&) -> bytebuf& = delete;
    auto operator=(bytebuf&& o) -> bytebuf&;
    ~bytebuf();

    auto send(buf data, u32 timeout) const -> bool;
    auto receive(u32 timeout) const -> buf;
    auto free(void* data) const -> void;

    auto capacity() const -> st;
    auto occupancy() const -> st;

    auto initialized() const -> bool;

private:
    void* impl = nullptr;
};


/* --- queue_t stuff --- */

struct lm::fabric::queue_t
{
    queue_t() = default;
    queue_t(st max_elements, st element_size_in_bytes);
    queue_t(queue_t const&) = delete;
    queue_t(queue_t&& o);
    auto operator=(const queue_t&) -> queue_t& = delete;
    auto operator=(queue_t&& o) -> queue_t&;

    template <typename As>
    struct consume_as
    {
        queue_t* q;

        struct iterator;
        auto begin() -> iterator;
        auto end()   -> iterator;
    };
    // Allows you to iterate over typed elements in a forloop.
    template <typename As>
    auto consume() -> consume_as<As>;

    /// TODO: Document what unit is this timeout in.
    auto send(void const* item, u32 timeout) -> bool;
    auto receive(void* into, u32 timeout) -> bool;

    auto capacity() const -> st;
    auto element_size() const -> st;

    ~queue_t();

private:
    void* impl = nullptr;
    st element_size_in_bytes = 0;
    st max_elements = 0;
};

template <typename T>
auto lm::fabric::queue(st max_elements) -> queue_t
{ return queue_t(max_elements, sizeof(T)); }

/* --- queue_t::view stuff. --- */

template <typename As> struct lm::fabric::queue_t::consume_as<As>::iterator
{
    queue_t* q;

    As current_element;
    bool is_done;

    // Iterator requirements for range-based for
    auto operator*() -> As& { return current_element; }
    auto operator++() -> iterator&
    {
        if(!q || !q->receive(&current_element, 0)) is_done = true;
        return *this;
    }
    auto operator!=(const iterator& o) const -> bool { return is_done != o.is_done; }
};
template <typename As>
auto lm::fabric::queue_t::consume() -> consume_as<As>
{ return consume_as<As>{this}; }
template <typename As>
auto lm::fabric::queue_t::consume_as<As>::begin() -> iterator
{ return iterator{q, {}, false}.operator++(); }
template <typename As>
auto lm::fabric::queue_t::consume_as<As>::end()   -> iterator
{ return {nullptr, {}, true}; }

/* --- semaphore --- */

struct lm::fabric::semaphore
{
    static auto create_binary() -> semaphore;
    static auto create_counting(u32 max, u32 initial) -> semaphore;

    semaphore() = default;
    semaphore(const semaphore&) = delete;
    auto operator=(const semaphore&) -> semaphore& = delete;
    semaphore(semaphore&& o) noexcept;
    auto operator=(semaphore&& o) noexcept -> semaphore&;

    ~semaphore();

    auto take(u32 timeout_ms = 0xFFFFFFFF) const -> bool;
    auto give() const -> void;
    // auto give_from_isr() const -> void;

    auto initialized() const -> bool;

private:
    void* impl = nullptr;
};
