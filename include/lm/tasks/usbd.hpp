#pragma once

#include "lm/fabric.hpp"

#include "lm/usbd/usbd.hpp"

namespace lm::tasks
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

        usbd(fabric::task_runtime_info& info);
        auto on_ready()     -> fabric::managed_task_status;
        auto before_sleep() -> fabric::managed_task_status;
        auto on_wake()      -> fabric::managed_task_status;
        ~usbd();

        auto broadcast_status() -> void;

        lm::usbd::configuration_t cfg;
        std::span<lm::usbd::endpoint_info_t> endpoints;
        lm::usbd::descriptors_t descriptors;

        fabric::queue_t get_status_q;
        fabric::bus::subscribe_token get_status_q_tok;

        fabric::task::task_handle_t tud_task_handle = nullptr;
    };
}
