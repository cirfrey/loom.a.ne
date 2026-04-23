#pragma once

#include "lm/core/types.hpp"

#include <new> // You know it's hardcore memory wibbly-wobbly library stuff when you need <new>.
#include <cstring> // For memcpy.

namespace lm::fabric
{
    namespace event_versions {
        struct [[gnu::packed]] v0 {
            static constexpr auto protocol_version = 0;
            // Protocol metadata.
            u8 version = protocol_version;
            u8 size    = sizeof(v0); // Useful if you receive an event with
                                     // a version you don't even know about and just want to step over it.
                                     // Also used for event extensions, must always be multiple of sizeof(v0),
                                     // but this is handled by the bus.

            // Event metadata.
            u8 topic;
            u8 type;
            u8 loom_id = 0;
            u8 strand_id;
            alignas(8) u64 timestamp = 0; // In microseconds.

            // Payload (what is actually being sent).
            alignas(8) u8 payload[8] = {0};

            constexpr auto is_local()        const -> bool { return loom_id == 0; }
            constexpr auto extension_count() const -> st   { return (size / sizeof(v0)) - 1; }

            // Payload management stuff.
            template <typename As>
            auto get_payload() const -> As const& {
                static_assert(sizeof(As) <= sizeof(payload), "Payload type too large");
                return *std::launder(reinterpret_cast<As const*>(payload));
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

            // Extension management stuff.
            template <typename Extension>
            static constexpr auto extension(Extension&& e) -> v0
            {
                static_assert(sizeof(Extension) <= sizeof(v0), "Extension type too large");
                static_assert(alignof(Extension) <= alignof(v0), "Extension type has too large alignment for this");
                v0 ev;
                std::memcpy(&ev, &e, sizeof(Extension));
                return ev;
            }
            template <typename As>
            auto as_extension() const -> As const& {
                static_assert(sizeof(As) <= sizeof(v0), "Extension type too large");
                static_assert(alignof(As) <= alignof(v0), "Extension type has too large alignment for this");
                return *std::launder(reinterpret_cast<As const*>(this));
            }
            template <typename As>
            auto as_extension() -> As& {
                static_assert(sizeof(As) <= sizeof(v0), "Extension type too large");
                static_assert(alignof(As) <= alignof(v0), "Extension type has too large alignment for this");
                return *std::launder(reinterpret_cast<As*>(this));
            }
        };
        static_assert(sizeof(v0) == 24, "loom.a.ne Event v0 must be exactly 24 bytes");
    }
    using event = event_versions::v0;

    namespace framework_topic_versions
    {
        namespace v0 {
            namespace types { enum types : u8
            {
                strand_status,
                strand_loop_timing,
                strand_signal,

                request_manager_announce,
                response_manager_announce,

                request_register_strand, // Has extensions, refer to the payload type.
                response_register_strand,

                request_strand_running,
                response_strand_running,
            }; }

            struct strand_status
            {
                static constexpr auto type = types::strand_status;
                enum status_t : u8 { not_created, created, ready, running, stopped, reaped } status;
            }; static_assert(sizeof(strand_status) <= sizeof(fabric::event::payload));

            struct strand_loop_timing
            {
                static constexpr auto type = types::strand_loop_timing;
                u64 time; // In microseconds.
            }; static_assert(sizeof(strand_loop_timing) <= sizeof(fabric::event::payload));

            struct strand_signal
            {
                static constexpr auto type = types::strand_signal;
                u8 target_strand;
                enum signal_t { start, stop, die } signal;
            }; static_assert(sizeof(strand_signal) <= sizeof(fabric::event::payload));

            struct request_manager_announce
            {
                static constexpr auto type = types::request_manager_announce;
                u8 seqnum;
            }; static_assert(sizeof(request_manager_announce) <= sizeof(fabric::event::payload));

            struct response_manager_announce
            {
                static constexpr auto type = types::response_manager_announce;
                u8 seqnum;
                u16 safe_timeout; // What's a safe timeout for this manager to ensure it gets any events we throw at it.
                u16 available_slots;
            }; static_assert(sizeof(response_manager_announce) <= sizeof(fabric::event::payload));

            // A register_strand_event is:
            // - 1 header with general data.
            // - 1 extension code and some depends (extdata).
            // - 1 extension with the name (extname).
            // - N extensions with the depends, where N >= 0 (extdepends).
            struct request_register_strand
            {
                static constexpr auto type = types::request_register_strand;

                static constexpr auto min_extension_count = 2;

                using name_t   = char[sizeof(fabric::event)];
                using strand_t = void(*)(void*);

                u8  seqnum = 0;
                u8  manager_id;
                u8  id = 0; // Only used if autoassign_id is false.
                bool autoassign_id = true;
                u32 timeout_micros = 10000; // If the manager has taken longer than this to register it, the attempt is dropped.

                struct depends_t {
                    u32 other = 0; // If 0, this depend is ignored. The manager will use this to match against either ID or fnv1a_32(name).
                    strand_status::status_t my_status;
                    strand_status::status_t other_status;
                };
                static constexpr auto depends_per_ext = sizeof(fabric::event) / sizeof(depends_t);

                static constexpr auto no_affinity = (u8)-1;
                // TODO: move me to config_t.
                static constexpr auto default_priority = 5;
                struct extdata {
                    u16 stack_size = 128;
                    u16 sleep_ms = 0;
                    bool request_running = false;
                    strand_t strand;
                    u8 priority      = default_priority;
                    u8 core_affinity = no_affinity;
                }; static_assert(sizeof(extdata) <= sizeof(fabric::event));

                struct extname {
                    name_t name;
                }; static_assert(sizeof(extname) <= sizeof(fabric::event));

                struct extdepends
                {
                    depends_t depends[depends_per_ext] = {};
                }; static_assert(sizeof(extdepends) <= sizeof(fabric::event));

            }; static_assert(sizeof(request_register_strand) <= sizeof(fabric::event::payload));

            // When you request_register_strand, the manager will respond using
            // this with the status and the same seqnum you passed.
            struct response_register_strand
            {
                static constexpr auto type = types::response_register_strand;

                enum result_t : u8
                {
                    ok,
                    request_malformed,
                    id_in_use,
                    name_in_use,
                    manager_cant_handle_more_strands,
                    too_many_depends,
                };

                u8 requester_id;
                u8 seqnum;

                result_t result;
                // Only valid if result = ok.
                u8 id;
            }; static_assert(sizeof(response_register_strand) <= sizeof(fabric::event::payload));

            struct request_strand_running
            {
                static constexpr auto type = types::request_strand_running;

                u8 seqnum;

                u8 strand_id;
                bool running_state = false;
            }; static_assert(sizeof(request_strand_running) <= sizeof(fabric::event::payload));

            struct response_strand_running
            {
                static constexpr auto type = types::response_strand_running;

                enum result_t : u8
                {
                    ok,
                    id_doesnt_exist,
                };

                u8 requester_id;
                u8 seqnum;

                result_t result;
                // Only valid if result = ok.
                bool running_state = false;
            }; static_assert(sizeof(response_strand_running) <= sizeof(fabric::event::payload));
        }
    }

    namespace topic_versions
    {
        // The ids from [0, reserved_topic_max] are reserved for loomane, your application can use any id from [free_topic_min, free_topic_max].
        // The event types themselves and payload they expect are defined within each module (USBD events are probably in usbd.hpp, etc...).
        // In event::v0 each topic can have up to 256 different event types per topic, so you have lots of space to make as many different events as
        // you would ever realistically need.
        namespace v0 {
            enum v0_t : u8 {
                any = 0, // Special topic signaling that "I want to listen to every topic".
                // Used by busmon or whoever wants to spy on the bus.
                framework, // Internals for loom.a.ne, lots of stuff regarding managed events and stuff like that.
                input, // Input events, like a button or some data from another loom. As in, something inputted INTO this loom.
                output, // Output events, like writing to uart or somewhere else. As in, output something OUT of this loom;
                busmon_teach, // Use this topic to "teach" busmon how to print messages of arbitrary topics.
                reserved_topic_max = busmon_teach,

                free_topic_min = reserved_topic_max,
                free_topic_max = 255,
            };

            namespace framework_t = framework_topic_versions::v0;
        }
    }
    namespace topic = topic_versions::v0;
}
