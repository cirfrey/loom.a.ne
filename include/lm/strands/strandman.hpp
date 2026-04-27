#pragma once

#include "lm/fabric/all.hpp"
#include "lm/config.hpp"

#include <span>

namespace lm::strands
{
    struct strandman
    {
        struct strand_t
        {
            using status_t  = fabric::topic::framework_t::strand_status::status_t;
            using depends_t = fabric::topic::framework_t::request_register_strand::depends_t;
            using name_t    = fabric::topic::framework_t::request_register_strand::name_t;

            fabric::strand::function_t code;
            u8 id;
            u16 stack_size; // In words
            u16 sleep_ms;
            u8 priority       = fabric::topic::framework_t::request_register_strand::default_priority;
            u16 core_affinity = fabric::topic::framework_t::request_register_strand::no_affinity;
            name_t name = "default.strand.name";
            depends_t depends[config_t::strandman_t::max_depends] = {};

            u64 status_timestamp = 0;
            status_t status      = status_t::not_created;

            fabric::strand::handle_t handle  = nullptr;
            bool requested_running           = false;
        };

        template <st MaxStrands>
        [[nodiscard]] static constexpr auto spawn(strand_t tman_info) -> fabric::strand::handle_t;

        strandman(st max_strands, std::span<strand_t>& strands);
        auto do_loop(st max_strands, std::span<strand_t>&) -> bool;

    private:
        // For a given strand and a given status, checks if they are free
        // to proceed to that status.
        auto dependencies_satisfied(
            std::span<strand_t> strands,
            strand_t const& strand,
            strand_t::status_t target_status
        ) -> bool;

        auto process_events(st, std::span<strand_t>&) -> void;
        auto manage_strands(st, std::span<strand_t>&) -> void;

        // Every one of these returns how many events/extensions it consumed.
        auto on_event_request_manager_announce(fabric::event const&, st, std::span<strand_t>&) -> st;
        auto on_event_request_manager_resolve(fabric::event const&,  st, std::span<strand_t>&) -> st;
        auto on_event_request_register_strand(fabric::event const&,  st, std::span<strand_t>&) -> st;
        auto on_event_request_strand_running(fabric::event const&,   st, std::span<strand_t>&) -> st;

        auto get_strand_named(u32 name_hash, std::span<strand_t>&) -> u8;

        fabric::queue_t status_q;
        fabric::bus::subscribe_token status_q_tok;

        fabric::queue_t strandman_q;
        fabric::bus::subscribe_token strandman_tok;
    };
}

template <lm::st MaxStrands>
constexpr auto lm::strands::strandman::spawn(strand_t strandman_info) -> fabric::strand::handle_t
{
    static_assert(MaxStrands > 0);

    auto sync = fabric::semaphore::create_binary();

    struct boot_payload {
        strand_t& strandman_info;
        fabric::semaphore& sync;
    } boot{strandman_info, sync};

    // We copy the boot info into the strand and then
    // everything kicks off from there.
    // But we need to make sure that boot_payload doesn't
    // go out of scope before the strand has it, so that's
    // why we need the semaphore.
    auto handle = fabric::strand::create(
        {
            .name = strandman_info.name,
            .id = strandman_info.id,
            .priority = strandman_info.priority,
            .stack_size = strandman_info.stack_size,
            .core_affinity = strandman_info.core_affinity,
            .sleep_ms = strandman_info.sleep_ms,
            .code = [](void* p){
                auto* boot = static_cast<boot_payload*>(p);

                strand_t strands[MaxStrands];
                strands[0] = boot->strandman_info;
                boot->sync.give(); // Release the semaphore.

                auto strandview = std::span(strands, 1);

                strandman man(MaxStrands, strandview);
                while(man.do_loop(MaxStrands, strandview));

                // TODO: how do we clean up tman?
                //fabric::strand::reap(fabric::strand::get_my_own_handle());
            },
        },
        &boot
    );

    // Block until the strand is done copying.
    sync.take();

    return handle;
}
