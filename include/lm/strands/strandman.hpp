#pragma once

#include "lm/fabric.hpp"

#include "lm/core/math.hpp"

#include <array>
#include <span>

namespace lm::strands
{
    struct strandman
    {
        struct strand_info_t
        {
            enum strand_status
            {
                not_created,
                requested_creation,
                created,
                ready,
                running,
                stopped,
                deleted,
            };

            using strand_id_t = decltype(fabric::strand_runtime_info::id);

            fabric::strand::function_t code;

            fabric::strand_constants    constants;
            fabric::strand_runtime_info runtime;

            struct depends_t {
                strand_status my_status;
                strand_id_t   other_id;
                strand_status other_status;
            };
            // TODO: configurable max depends
            depends_t depends[32] = {};

            strand_status status             = strand_status::not_created;
            fabric::strand::handle_t handle  = nullptr;
            bool should_be_running           = false;
        };

        template <st MaxStrands>
        static constexpr auto spawn(
            strand_info_t tman_info,
            std::span<strand_info_t> strands
        ) -> void;

        strandman();
        auto do_loop(st max_strands, std::span<strand_info_t>&) -> bool;

    private:
        // For a given strand and a given status, checks if they are free
        // to proceed to that status.
        auto dependencies_satisfied(
            std::span<strand_info_t>& strands,
            strand_info_t strand,
            strand_info_t::strand_status target_status
        ) -> bool;

        auto process_events(std::span<strand_info_t>&) -> void;
        auto spawn_strands(std::span<strand_info_t>&) -> void;
        auto set_running_strands(std::span<strand_info_t>&) -> void;
        auto reap_stopped_strands(std::span<strand_info_t>&) -> void;

        fabric::queue_t status_q;
        fabric::bus::subscribe_token status_q_tok;
    };
}

template <lm::st MaxStrands>
constexpr auto lm::strands::strandman::spawn(
    strand_info_t tman_info,
    std::span<strand_info_t> strands
) -> void
{
    auto sync = fabric::semaphore::create_binary();

    struct boot_payload {
        strand_info_t& tman_info;
        std::span<strand_info_t>& strands;
        fabric::semaphore& sync;
    } boot{tman_info, strands, sync};

    // We copy the boot info into the strand and then
    // everything kicks off from there.
    fabric::strand::create(
        tman_info.constants,
        tman_info.runtime,
        [](void* p){
            auto* boot = static_cast<boot_payload*>(p);

            std::array<strand_info_t, MaxStrands> strands;
            strands[0] = boot->tman_info;
            st i = 1;
            for(auto& t : boot->strands) {
                if (i >= MaxStrands) break; // Safety guard
                strands[i++] = t;
            }
            boot->sync.give(); // Release the parent.

            auto strandview = std::span(strands.data(), clamp(boot->strands.size() + 1, 0, MaxStrands));

            strandman man;
            while(man.do_loop(MaxStrands, strandview));

            fabric::strand::reap(fabric::strand::get_handle());
    }, &boot);

    // Block until the child is done copying
    sync.take();
}
