#pragma once

#include "lm/fabric.hpp"

#include "lm/usbd/usbd.hpp"

namespace lm::strands
{
    struct usbd
    {
        struct event
        {
            enum event_t : u8
            {
                get_status,

                cdc_enabled,
                cdc_disabled,
                hid_enabled,
                hid_disabled
            };
        };

        usbd(fabric::strand_runtime_info& info);
        auto on_ready()     -> fabric::managed_strand_status;
        auto before_sleep() -> fabric::managed_strand_status;
        auto on_wake()      -> fabric::managed_strand_status;
        ~usbd();

        auto broadcast_status() -> void;

        lm::usbd::configuration_t cfg;
        std::span<lm::usbd::endpoint_info_t> endpoints;
        lm::usbd::descriptors_t descriptors;

        fabric::queue_t get_status_q;
        fabric::bus::subscribe_token get_status_q_tok;

        fabric::strand::handle_t tud_strand_handle = nullptr;
    };
}