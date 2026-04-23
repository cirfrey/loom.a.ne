// Maybe it's a smart idea to abstract from freeRTOS... Maybe not, I don't know yet.
#pragma once

#include "lm/core/types.hpp"
#include "lm/core/cvt.hpp"
#include "lm/fabric/primitives.hpp"
#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"

// The main api surface for dealing with strands.
namespace lm::fabric::strand
{
    using function_t = void (*)(void*);

    using handle_t = void*;
    constexpr handle_t bad_handle  = nullptr;

    struct create_strand_args
    {
        char const* name;
        u8 id;
        st priority;
        st stack_size;
        st core_affinity = no_affinity; // What core it should run at.
        u16 sleep_ms;

        // You only need to set this if you are using the non-templated version of lm::fabric::strand::create.
        function_t code = nullptr;

        // The strand can run at any core.
        static constexpr auto no_affinity = (st)-1;
    };
    // This is the one you probably want to use:
    template <typename Strand>
    [[nodiscard]] constexpr auto create(create_strand_args args) -> handle_t;
    // Create a strand given some args. raw_params is forwarded to the strand.
    [[nodiscard]] auto create(create_strand_args const& args, void* params = nullptr) -> handle_t;

    // Deltas and timestamps are in micros.
    // requested_sleep_ms is in millis.
    struct strand_runtime_info
    {
        u64 created_timestamp = 0;
        u64 running_timestamp = 0;

        u8 id = 0;
        u16 requested_sleep_ms = 0;

        u64 actual_sleep_delta = 0;

        u64 last_before_sleep_delta = 0;
        u64 last_on_wake_delta = 0;
    };

    enum managed_strand_status
    {
        ok,
        suicidal,
    };
}

// Strand utilities.
namespace lm::fabric::strand
{
    // Yield for atleast N milliseconds. If N == 0 just yield to something of equal or higher priority if
    // theres someone waiting to run, otherwise keep running.
    auto sleep_ms(unsigned long ms) -> void;

    // Get your own strand handle. You probably never want to do this, but you never know.
    [[nodiscard]] auto get_my_own_handle() -> handle_t;

    enum class reap_status { success, error };
    // Deletes a strand by it's handle. The strand must want to be deleted.
    [[nodiscard]] auto reap(handle_t handle) -> reap_status;
}

#include <cstdio>
// The *actual* API behind all the managed strand things. You probably don't want to use any of this directly.
namespace lm::fabric::strand
{
    /** --- A wrapper to make tasking easy ---
     * This is the main way you might interact with the strand system.
     * Here's what it expects your strand to look like.
     *   struct my_strand
     *   {
     *       my_strand(lm::fabric::strand::strand_runtime_info& info) {}
     *       auto on_ready() {} > lm::fabric::strand::managed_strand_status { return lm::fabric::strand::managed_strand_status::ok; }
     *       auto before_sleep() -> lm::fabric::strand::managed_strand_status { return lm::fabric::strand::managed_strand_status::ok; }
     *       auto on_wake() -> lm::fabric::strand::managed_strand_status { return lm::fabric::strand::managed_strand_status::ok; }
     *       ~my_strand() {}
     *   }
     * At the end of the day this is just a wrapper to lm::fabric::strand::manage(...).
     */
    template <typename Strand>
    constexpr auto managed() -> function_t;

    struct managed_strand_params
    {
        u8 id;
        u16 sleep_ms;
    };
    static_assert(sizeof(managed_strand_params) <= sizeof(void*), "managed_strand_params needs to fit in a void* to be smuggled across the backend");

    struct type_erased_strand_t
    {
        using construct_t = void(*)(void* ud, strand_runtime_info&);
        using destruct_t = void(*)(void* ud);
        using cb = managed_strand_status(*)(void* ud);

        void* ud = nullptr;
        bool constructed = false;
        construct_t construct_cb = nullptr;
        destruct_t destruct_cb   = nullptr;
        cb on_ready_cb     = nullptr;
        cb before_sleep_cb = nullptr;
        cb on_wake_cb      = nullptr;

        auto construct(strand_runtime_info& info) -> void
        {
            if(constructed || !construct_cb) return;
            construct_cb(ud, info);
            constructed = true;
        }
        constexpr auto on_ready()     -> managed_strand_status
        { if(constructed && on_ready_cb)     return on_ready_cb(ud);     return managed_strand_status::ok; }
        constexpr auto before_sleep() -> managed_strand_status
        { if(constructed && before_sleep_cb) return before_sleep_cb(ud); return managed_strand_status::ok; }
        constexpr auto on_wake()      -> managed_strand_status
        { if(constructed && on_wake_cb)      return on_wake_cb(ud);      return managed_strand_status::ok; }
        constexpr ~type_erased_strand_t()
        { if(constructed && destruct_cb) destruct_cb(ud); }

    };
    auto manage(void* params, type_erased_strand_t type_erased_strand) -> void;

    /// Managed strand lifetime stuff.

    // Assumes the queue is actually subscribed and listening to the signals.
    auto wait_for_running(queue_t&, u8 strandid) -> void;
    // Assumes the queue is actually subscribed and listening to the signals.
    auto should_stop(queue_t&, u8 strandid) -> bool;
    auto wait_for_shutdown(queue_t&, u8 strandid) -> void;
}

/* --- Implementations --- */

template <typename Strand>
constexpr auto lm::fabric::strand::managed() -> function_t
{
    return [](void* params){
        alignas(Strand) char buffer[sizeof(Strand)];

        type_erased_strand_t te {
            .ud = buffer,
            .construct_cb    = [](void* buffer, strand_runtime_info& info){ new (buffer) Strand(info); },
            .destruct_cb     = [](void* buffer){ (buffer | rc<Strand*>)->~Strand(); },
        };

        #define LM_FABRIC_STRAND_BIND_IF_EXISTS(func_name, cb_member) \
        if constexpr (requires(Strand s) { { s.func_name() } -> veil::convertible_to<managed_strand_status>; }) { \
            te.cb_member = [](void* b){ return reinterpret_cast<Strand*>(b)->func_name(); }; \
        }
        LM_FABRIC_STRAND_BIND_IF_EXISTS(on_ready,     on_ready_cb)
        LM_FABRIC_STRAND_BIND_IF_EXISTS(before_sleep, before_sleep_cb)
        LM_FABRIC_STRAND_BIND_IF_EXISTS(on_wake,      on_wake_cb)
        #undef LM_FABRIC_STRAND_BIND_IF_EXISTS

        manage(params, te);
    };
}

template <typename Strand>
constexpr auto lm::fabric::strand::create(create_strand_args args) -> handle_t
{
    args.code = managed<Strand>();
    return create(args, managed_strand_params{ .id = args.id, .sleep_ms = args.sleep_ms } | smuggle<void*>);
}
