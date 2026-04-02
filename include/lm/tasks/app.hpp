#pragma once

namespace lm::task { struct config; } // Forward decl.

namespace lm::app
{
    /// TODO: this needs some looking into.
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

    auto task(lm::task::config const& cfg) -> void;
    auto init() -> void;
}
