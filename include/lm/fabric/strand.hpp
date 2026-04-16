// Maybe it's a smart idea to abstract from freeRTOS... Maybe not, I don't know yet.
#pragma once

#include "lm/core/types.hpp"
#include "lm/core/cvt.hpp"
#include "lm/fabric/primitives.hpp"
#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"

namespace lm::fabric::strand
{
    using function_t = void (*)(void*);
    using handle_t   = void*;

    /** --- A wrapper to make tasking easy ---
     * This is the main way you might interact with the strand system.
     * Here's what it expects your strand to look like.
     *   struct my_strand
     *   {
     *       my_strand(lm::fabric::strand_runtime_info& info) {}
     *       auto on_ready() {} > lm::fabric::managed_strand_status { return lm::fabric::managed_strand_status::ok; }
     *       auto before_sleep() -> lm::fabric::managed_strand_status { return lm::fabric::managed_strand_status::ok; }
     *       auto on_wake() -> lm::fabric::managed_strand_status { return lm::fabric::managed_strand_status::ok; }
     *       ~my_strand() {}
     *   }
     */
    template <typename Strand>
    constexpr auto managed() -> function_t;


    /**  --- Managed strand lifetime stuff. ---
     * if you use managed() you won't need this. This is only if you implement raw strands that interface with
     * fabric::strand::create().
     */

    // Assumes the queue is actually subscribed and listening to the signals.
    auto wait_for_start(queue_t&, st strandid) -> void;
    // Assumes the queue is actually subscribed and listening to the signals.
    auto should_stop(queue_t&, st strandid) -> bool;
    auto wait_for_shutdown(st strandid) -> void;

    auto sleep_ms(unsigned long ms) -> void;

    // When you call lm::strand::create(lm::config::whatever, lm::whatever::strand, (void*)whateverparams).
    // void* whateverparams is forwarded to lm::whatever::strand.
    auto create(strand_constants const& cfg, strand_runtime_info const& info, function_t strand, void* strandarg = nullptr) -> handle_t;
    auto get_handle() -> handle_t;
    // Deletes a strand by it's handle.
    auto reap(handle_t handle) -> void;

    enum event : u8
    {
        /* --- status updates (payload = status_event) --- */

        created, // Handled by lm::strand::create.
        ready,
        running,
        waiting_for_reap,



        loop_timing,


        /* --- signals (payload = signal_event) --- */

        signal_start,
        signal_stop,
    };

    struct status_event
    {
        void* handle;
    }; static_assert(sizeof(status_event) <= sizeof(fabric::event::payload));

    struct timing_event
    {
        u64 time;
    }; static_assert(sizeof(timing_event) <= sizeof(fabric::event::payload));

    struct signal_event
    {
        u64 strand_id;
    }; static_assert(sizeof(signal_event) <= sizeof(fabric::event::payload));
}

#include "lm/chip/time.hpp"

/* --- Impls --- */
template <typename Strand>
constexpr auto lm::fabric::strand::managed() -> function_t
{
    return [](void* param){
        auto info = (param | smuggle<strand_runtime_info_pun>).raw;

        [&]{
            Strand strand{info};

            auto signal_queue = fabric::queue<fabric::event>(8);
            auto signal_token = bus::subscribe(
                signal_queue,
                topic::strand,
                {event::signal_start, event::signal_stop}
            );
            wait_for_start(signal_queue, info.id);

            if(strand.on_ready() == managed_strand_status::suicidal) return;
            auto start = chip::time::uptime();
            while (!should_stop(signal_queue, info.id))
            {
                if(strand.before_sleep() == managed_strand_status::suicidal) return;

                auto end = chip::time::uptime();
                bus::publish(fabric::event{
                    .topic = topic::strand,
                    .type  = event::loop_timing,
                    .sender_id = info.id,
                }.with_payload(timing_event{ .time = end - start }));
                sleep_ms(info.sleep_ms);
                start = chip::time::uptime();

                if(strand.on_wake() == managed_strand_status::suicidal) return;
            }
        }();

        wait_for_shutdown(info.id);
    };
}
