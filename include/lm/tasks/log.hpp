#pragma once

#include "lm/fabric.hpp"
#include "lm/config.hpp"

#include "lm/utils/stopwatch.hpp"

#include <array>

namespace lm::tasks::logging
{
    struct consumer
    {
        enum class type_t
        {
            disabled,
            uart,
            hid,
            cdc,
        };

        enum class status
        {
            consuming,
            done,
            blocked,
        };

        auto consume(buf) -> status;
        auto flush()      -> void;

        type_t type = type_t::disabled;
    private:
        // Refers to the buf it is currently consuming.
        auto mark_as_done() -> status;
        st offset        = 0;
        st total_chunks  = 0;
        st failed_chunks = 0;
    };

    struct log
    {
        st refcount = 0; // How many consumers are still holding on to this log.
        buf data    = {};
    };

    // Very simple ringbuf with basically no safety or concurrency stuff,
    // just to be used by the logging task internally.
    template <typename T, std::size_t Capacity>
    struct ring_buffer
    {
        using value_t = T;
        static constexpr auto capacity = Capacity;

        std::array<T, Capacity> data;
        size_t head = 0;   // Index of the front element
        size_t tail = 0;   // Index where the next element will be inserted
        size_t count = 0;  // Current number of elements

        u64 lifetime_elements = 0;

        bool empty() const { return count == 0; }
        bool full() const { return count == Capacity; }
        size_t size() const { return count; }

        void enqueue(const T& value) {
            data[tail] = value;
            tail = (tail + 1) % Capacity; // Wrap around
            count++;
            lifetime_elements++;
        }

        void dequeue() {
            head = (head + 1) % Capacity; // Wrap around
            count--;
        }

        auto oldest() -> T& { return data[head]; }

        auto get_by_id(u64 id) -> T& {
            // Find how far 'id' is from the first id currently in the buffer
            u64 first_id = lifetime_elements - count;
            u64 relative_offset = id - first_id;
            return data[(head + relative_offset) % Capacity];
        }
    };
}

namespace lm::tasks
{
    struct log
    {
        static auto dispatch(text t) -> bool;

        log(fabric::task_runtime_info& info);
        auto on_ready()     -> fabric::managed_task_status;
        auto before_sleep() -> fabric::managed_task_status;
        auto on_wake()      -> fabric::managed_task_status;
        ~log();

        logging::ring_buffer<logging::log, config::logging::consumerbuf_max_size> logs;
        static constexpr auto consumer_count = 4;
        u64 consumer_logids[consumer_count] = {0};
        logging::consumer consumers[consumer_count];

        fabric::queue_t usbd_status_q;
        fabric::bus::subscribe_token usbd_status_q_tok;

        using usbd_status_timer_t = simple_timer<
            std::chrono::seconds,
            std::chrono::high_resolution_clock
        >;
        usbd_status_timer_t usbd_status_timer = usbd_status_timer_t(10);
    };
}
