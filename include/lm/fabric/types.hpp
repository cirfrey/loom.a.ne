#pragma once

#include "lm/core/types.hpp"

#include <new> // You know it's hardcore memory wibbly-wobbly library stuff when you need <new>.
#include <cstring> // For memcpy.

namespace lm::fabric
{
    namespace event_versions {
        struct [[gnu::packed]] v0 {
            // Protocol metadata.
            u8 version = 0;
            u8 size    = sizeof(v0); // For avoiding version drift.
                                     // Useful if you receive an event with
                                     // a version you don't even know about and just want to step over it.
                                     // to the next event in line.

            // Event metadata.
            u8 topic;
            u8 type;
            u16 sender_id;
            u16 sequence_id;
            alignas(8) u64 timestamp = 0;

            // Payload (what is actually being sent).
            alignas(8) u8 payload[8] = {0};

            template <typename As>
            auto get_payload() const -> const As& {
                static_assert(sizeof(As) <= sizeof(payload), "Payload type too large");
                return *std::launder(reinterpret_cast<const As*>(payload));
            }
            template <typename As>
            auto get_payload() -> As& {
                static_assert(sizeof(As) <= sizeof(payload), "Payload type too large");
                return *std::launder(reinterpret_cast<As*>(payload));
            }
            template <typename Payload>
            auto with_payload(Payload&& p) -> v0&
            {
                static_assert(sizeof(Payload) <= sizeof(payload), "Payload type too large");
                std::memcpy(payload, &p, sizeof(Payload));
                return *this;
            }
        };
        static_assert(sizeof(v0) == 24, "loom.a.ne Event v0 must be exactly 24 bytes");
    }
    using event = event_versions::v0;

    namespace topic_versions
    {
        // The ids from [0, reserved_topic_max] are reserved for loomane, your application can use any id from [free_topic_min, free_topic_max].
        // The event types themselves and payload they expect are defined within each module (USBD events are probably in usbd.hpp, etc...).
        // In event::v0 each topic can have up to 256 different event types per topic, so you have lots of space to make as many different events as
        // you would ever realistically need.
        enum v0 : u8 {
            any = 0, // Special topic signaling that "I want to listen to every topic".
                     // Used by busmon or whoever wants to spy on the bus.
            /// TODO: Collapse as many of these topics together as we can and use the event type filter of the bus.
            ///       This makes it so we can give the users more playroom by setting a lower free_topic_min.
            task,
            blink,
            usbd,
            busmon_teach, // Use this topic to "teach" busmon how to print messages of arbitrary topics.
            reserved_topic_max = busmon_teach,

            free_topic_min = 16,
            free_topic_max = (u8)-1,
        };
    }
    using topic = topic_versions::v0;

    struct task_constants
    {
        char const* name;
        st priority;
        st stack_size;
        st core_affinity; // What core it should run at.

        // The task can run at any core.
        static constexpr auto no_affinity = (st)-1;
    };

    struct task_runtime_info
    {
        u8 id;
        u16 sleep_ms;
    };
    static_assert(sizeof(task_runtime_info) <= sizeof(void*), "task_runtime_info needs to fit in a void* to be smuggled across the backend");

    enum managed_task_status
    {
        ok,
        exit,
    };
}
