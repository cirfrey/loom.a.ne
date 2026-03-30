#pragma once

#include "lm/core/types.hpp"

namespace lm::task
{
    // Used for identifying the sender
    // in the bus and maybe other places.
    enum class id_t : u8
    {
        unspecified = 0,

        taskid_min = 1,

        app = taskid_min,
        blink,
        busmon,
        healthmon,
        logging,
        radio,
        sysman,
        usbd,

        taskid_max = usbd,
        taskid_count
    };

    struct config
    {
        id_t id;
        u8 priority;
        const char* name;
        u16 stack_size;
        u16 sleep_ms; // How much it should sleep between loops, or however it may sleep internally, if applicable.
        i16 core_affinity; // What core it should run at.

        // The task can run at any core.
        static constexpr auto no_affinity = -1;
    };
}
