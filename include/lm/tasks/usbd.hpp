#pragma once

#include "lm/fabric/types.hpp"

#include "lm/usbd/usbd.hpp"

namespace lm::tasks
{
    struct usbd
    {
        usbd(fabric::task_runtime_info& info);
        auto on_ready()     -> fabric::managed_task_status;
        auto before_sleep() -> fabric::managed_task_status;
        auto on_wake()      -> fabric::managed_task_status;
        ~usbd();

        lm::usbd::configuration_t cfg;
        std::span<lm::usbd::endpoint_info_t> endpoints;
        lm::usbd::descriptors_t descriptors;
    };
}
