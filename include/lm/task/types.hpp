#pragma once

#include "lm/aliases.hpp"

namespace lm::task
{
    enum class affinity
    {
        core_wifi,       // Logical Core A
        core_processing, // Logical Core B
        core_any,        // Dynamic/OS decided
    };

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

        taskid_max,
    };

    struct config
    {
        id_t id;
        u8 priority;
        const char* name;
        u16 stack_size;
        u16 sleep_ms; // How much it should sleep between loops, or however it may sleep internally, if applicable.
        lm::task::affinity core_affinity; // What core it should run at, remember that the ESP32S2 has 1 core and the ESP32S3 has 2.
    };
}
