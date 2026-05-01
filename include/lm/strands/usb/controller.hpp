#pragma once

#include "lm/fabric/all.hpp"

#include "lm/config.hpp"

namespace lm::strands::usb
{
    struct controller
    {
        using ri     = fabric::strand::strand_runtime_info;
        using status = fabric::strand::managed_strand_status;

        struct get_cfg_args
        {
            u8 id = 0;
            u64 tries = 3;
            u64 timeout = 0;
            u32 name_hash = 0; // If you already have the name_hash and want to just get
                               // the pointer.
        };
        static auto get_cfg(get_cfg_args) -> config_t::controller_t const*;

        controller(ri& info, config_t::controller_t const* cfg = nullptr);
        auto on_ready()     -> status;
        auto before_sleep() -> status;
        auto on_wake()      -> status;
        ~controller();

        ri& info;

        auto get_cfg() -> void;
        config_t::controller_t const* cfg;

        // This sits right before the controller so you can easily send it over via bus.
        fabric::event descriptor_event;
        alignas(fabric::event) u8 config_descriptor[config_t::controller_t::config_descriptor_max_size];

        // Maps eps to strand_ids.
        u8 ep_in_map[config_t::controller_t::epin_count];
        u8 ep_out_map[config_t::controller_t::epout_count];


        // For debouncing.
        u64 last_register_ns;
        bool reenumerate_pending;

        // Event stuff.
        fabric::queue_t q;
        fabric::bus::subscribe_token tok;
    };
}
