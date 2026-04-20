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

    using handle_t = void*;
    constexpr handle_t bad_handle  = nullptr;

    struct managed_strand_params
    {
        u8 id;
        u16 sleep_ms;
    };
    static_assert(sizeof(managed_strand_params) <= sizeof(void*), "managed_strand_params needs to fit in a void* to be smuggled across the backend");
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
    auto wait_for_running(queue_t&, u8 strandid) -> void;
    // Assumes the queue is actually subscribed and listening to the signals.
    auto should_stop(queue_t&, u8 strandid) -> bool;
    auto wait_for_shutdown(queue_t&, u8 strandid) -> void;

    auto sleep_ms(unsigned long ms) -> void;


    struct create_strand_args
    {
        char const* name;
        u8 id;
        st priority;
        st stack_size;
        st core_affinity; // What core it should run at.
        u16 sleep_ms;

        function_t code;

        // The strand can run at any core.
        static constexpr auto no_affinity = (st)-1;
    };
    // When you call lm::strand::create(lm::config::whatever, lm::whatever::strand, (void*)whateverparams).
    // void* whateverparams is forwarded to lm::whatever::strand.
    [[nodiscard]] auto create(create_strand_args const& args, void* raw_params = nullptr) -> handle_t;
    [[nodiscard]] auto get_handle() -> handle_t;

    // Deletes a strand by it's handle.
    enum class reap_status
    {
        success,
        error,
    };
    [[nodiscard]] auto reap(handle_t handle) -> reap_status;
}

#include "lm/chip/time.hpp"

/* --- Impls --- */
template <typename Strand>
constexpr auto lm::fabric::strand::managed() -> function_t
{
    return [](void* _params){
        auto params = _params | unsmuggle<managed_strand_params>;

        strand_runtime_info info {
            .created_timestamp = chip::time::uptime(),
            .id = params.id,
            .requested_sleep_ms = params.sleep_ms,
        };

        auto signal_queue = fabric::queue<fabric::event>(8);
        auto signal_token = bus::subscribe(signal_queue, topic::framework, {topic::framework_t::strand_signal::type});

        [&]{
            Strand strand{info};

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
    };
}
