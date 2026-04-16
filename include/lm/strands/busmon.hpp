#pragma once

#include "lm/core/types.hpp"
#include "lm/fabric.hpp"

namespace lm::strands
{
    struct busmon
    {
        using stringify_cb =
            st/*how many bytes you wrote*/(*)
            (fabric::event const& /*event*/, mut_text);
        struct teach_topic
        {
            stringify_cb stringify;
        };
        static_assert(sizeof(teach_topic) <= sizeof(fabric::event));

        busmon(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~busmon() = default;

    private:
        fabric::queue_t ev_q    = {}; // Listens to the whole event bus.
        fabric::queue_t teach_q = {}; // Listens specifically for teach events.
        fabric::bus::subscribe_token ev_q_tok    = {};
        fabric::bus::subscribe_token teach_q_tok = {};
        stringify_cb topic_cbs[fabric::topic::free_topic_max] = {nullptr};
    };
}
