#pragma once

#include "lm/core/types.hpp"

#include <new> // You know it's hardcore memory wibbly-wobbly library stuff when you need <new>.
#include <cstring> // For memcpy.

namespace lm::fabric
{
    namespace event_versions {
        struct [[gnu::packed]] v0 {
            static constexpr auto protocol_version = 0;
            static constexpr auto protocol_size = 24;

            static constexpr auto loom_bits = 6;
            static constexpr auto mesh_bits = 2;

            static constexpr auto local_loom = 0;
            static constexpr auto unidentified_strand = 0;

            // Protocol metadata.
            u8 version = protocol_version;
            u8 size    = protocol_size; // Useful if you receive an event with
                                        // a version you don't even know about and just want to step over it.
                                        // Also used for event extensions, must always be multiple of sizeof(v0),
                                        // but this is handled by the bus.

            // Event metadata.
            u8 topic;
            u8 type;
            u8 strand_id = unidentified_strand;
            u8 loom_id : loom_bits = local_loom;
            u8 mesh_id : mesh_bits = 0;
            alignas(8) u64 timestamp = 0; // In microseconds.

            // Payload (what is actually being sent).
            alignas(8) u8 payload[8] = {0};

            constexpr auto is_local()        const -> bool { return loom_id == local_loom; }
            constexpr auto extension_count() const -> st {
                auto rounded_up = ((size + protocol_size - 1) / protocol_size) * protocol_size;
                return rounded_up - 1;
            }
            constexpr auto extension_bytes() const -> st { return size - protocol_size; }

            // Payload management stuff.
            template <typename As>
            constexpr auto get_payload() const -> As const& {
                static_assert(sizeof(As) <= sizeof(payload), "Payload type too large");
                return *std::launder(reinterpret_cast<As const*>(payload));
            }
            template <typename As>
            constexpr auto get_payload() -> As& {
                static_assert(sizeof(As) <= sizeof(payload), "Payload type too large");
                return *std::launder(reinterpret_cast<As*>(payload));
            }
            template <typename Payload>
            constexpr auto with_payload(Payload&& p) -> v0&
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
                std::memcpy(reinterpret_cast<void*>(&ev), &e, sizeof(Extension));
                return ev;
            }
            template <typename As>
            constexpr auto as_extension() const -> As const& {
                static_assert(sizeof(As) <= sizeof(v0), "Extension type too large");
                static_assert(alignof(As) <= alignof(v0), "Extension type has too large alignment for this");
                return *std::launder(reinterpret_cast<As const*>(this));
            }
            template <typename As>
            constexpr auto as_extension() -> As& {
                static_assert(sizeof(As) <= sizeof(v0), "Extension type too large");
                static_assert(alignof(As) <= alignof(v0), "Extension type has too large alignment for this");
                return *std::launder(reinterpret_cast<As*>(this));
            }
        };
        static_assert(sizeof(v0) == v0::protocol_size, "loom.a.ne Event v0 must be exactly 24 bytes");
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

                request_manager_resolve,
                response_manager_resolve,

                request_manager_announce,
                response_manager_announce,

                request_register_strand, // Has extensions, refer to the payload type.
                response_register_strand,

                request_strand_running,
                response_strand_running,

                // ───────────────────────────────────────────────────────────────────────────
                //  USB Control Protocol — 4 events
                // ───────────────────────────────────────────────────────────────────────────
                //
                //  Participants:
                //    Device     — produces/consumes endpoint data (MIDI, HID, CDC, libusb proxy, ...)
                //    Transport  — drives a real or virtual USB bus (TinyUSB, USB-IP, dummy HCD, ...)
                //    Aggregator — sits between them in composite/hub mode; speaks both sides
                //
                //  The protocol is symmetric. An Aggregator is a Transport to its Devices
                //  and a Device to its Transport. Passthrough needs no Aggregator.
                //
                // ───────────────────────────────────────────────────────────────────────────
                //
                //  usb_ctrl_capability  —  connection lifecycle
                //
                //    status: announcing | attach | detach | reenumeration
                //    role:   device | transport
                //
                //    Transport binds to Device/Aggregator:
                //      Transport  → {announcing, transport}
                //      Device     → {attach, device}       — accepted
                //                 → {detach, device}       — refused (e.g. endpoint budget)
                //
                //    Device binds to Aggregator (same flow, roles swapped):
                //      Device     → {announcing, device}
                //      Aggregator → {attach, aggregator} / {detach, aggregator}
                //
                //    Either side detaches:
                //      any        → {detach, <role>}
                //
                //    Aggregator triggers reenumeration (descriptor tree changed):
                //      Aggregator → {reenumeration, device}   — Transport reconnects bus, no reply expected
                //
                // ───────────────────────────────────────────────────────────────────────────
                //
                //  usb_ctrl_setup_request  —  EP0 control transfer from host
                //
                //    Transport/Aggregator → Device/Aggregator.
                //    Carries the raw USB setup packet. wLength is implicit in extension_bytes().
                //    Only one in flight at a time — EP0 state machine enforces ordering.
                //    Device responds with usb_ctrl_data on EP0, targeting event.strand_id.
                //
                // ───────────────────────────────────────────────────────────────────────────
                //
                //  usb_ctrl_data  —  endpoint data, both directions; also EP0 responses
                //
                //    ep == 0  (control response):
                //      stall=0, ep_info=control_has_data  → ACK with data in extensions
                //      stall=0, ep_info=control_no_data   → ACK, zero-length status stage
                //      stall=1                            → STALL
                //
                //    ep != 0  (bulk / interrupt / iso):
                //      ep_info = iso | bulk | interrupt
                //      ep_direction = in (device→host) | out (host→device)
                //      data[4] + extensions carry the payload
                //
                // ───────────────────────────────────────────────────────────────────────────
                //
                //  usb_ctrl_interface_status  —  interface state change notification
                //
                //    Sent by Transport/Aggregator to Device after SET_CONFIGURATION,
                //    SET_INTERFACE, or suspend/resume. Keeps Devices from parsing
                //    standard USB requests themselves.
                //
                // ───────────────────────────────────────────────────────────────────────────
                //
                //  Typical flow (passthrough, Device ↔ Transport):
                //
                //    Transport  → capability {announcing}
                //    Device     → capability {attach}
                //    Transport  → setup_request            GET_DESCRIPTOR(device)
                //    Device     → data {EP0, has_data}     device descriptor
                //    Transport  → setup_request            SET_CONFIGURATION
                //    Device     → data {EP0, no_data}
                //    Transport  → interface_status         {interface 0, configured}
                //    Device     → data {EP1 IN}            MIDI note on
                //    Transport  → data {EP1 OUT}           MIDI note on from host
                //    Transport  → capability {detach}
                //
                // ───────────────────────────────────────────────────────────────────────────
                //  Typical flow (composite, Device ↔ Aggregator ↔ Transport):
                //
                //    -- Aggregator binds its Device --
                //    Device       → capability {announcing, device}
                //    Aggregator   → capability {attach, aggregator}
                //
                //    -- Transport binds to Aggregator (Aggregator acts as Device) --
                //    Transport    → capability {announcing, transport}
                //    Aggregator   → capability {attach, device}
                //
                //    -- Host enumerates (Transport forwards to Aggregator, Aggregator routes internally) --
                //    Transport    → setup_request              GET_DESCRIPTOR(device)
                //    Aggregator   → data {EP0, has_data}       composite device descriptor
                //    Transport    → setup_request              GET_DESCRIPTOR(config)
                //    Aggregator   → data {EP0, has_data}       composite config descriptor (stitched)
                //    Transport    → setup_request              SET_CONFIGURATION
                //    Aggregator   → data {EP0, no_data}
                //    Aggregator   → interface_status           {interface 0, configured}   → Device
                //
                //    -- Runtime data --
                //    Device       → data {EP1 IN}              MIDI note on   → Aggregator remaps → Transport
                //    Transport    → data {EP2 OUT}             HID report     → Aggregator remaps → Device
                //
                //    -- Device detaches, Aggregator rebuilds and reenumerates --
                //    Device       → capability {detach, device}
                //    Aggregator   → capability {reenumeration, device}        → Transport reconnects bus
                //
                // ───────────────────────────────────────────────────────────────────────────
                usb_ctrl_capability,
                usb_ctrl_data,
                usb_ctrl_setup_request,
                usb_ctrl_interface_status,
            }; }

            struct usb_ctrl_capability
            {
                enum status_t : u8 { announcing = 0, reenumeration = 1, attach = 2, detach = 3 };
                enum role_t : u8 { device = 0, transport = 1 };

                static constexpr auto type = types::usb_ctrl_capability;
                u8 target_strand_id;
                u8 target_loom_id : event::loom_bits;
                u8 target_mesh_id : event::mesh_bits;

                u8 ep_in_count : 4;
                u8 ep_out_count : 4;

                // bool supports_control : 1; // Is implicitly always true, so we don't need it.
                bool supports_iso : 1;
                bool supports_bulk : 1;
                bool supports_interrupt : 1;
                role_t role : 1;
                status_t status : 2;

                u8 seqnum;
            }; static_assert(sizeof(usb_ctrl_capability) <= sizeof(fabric::event::payload));

            struct usb_ctrl_data
            {
                enum direction_t : u8 { in = 0, out = 1 };
                enum ep_info_t : u8 {
                    // When ep == 0 its always a control endpoint, so we use the bits for this other signal.
                    control_has_data = 0,
                    control_no_data = 1,

                    // Otherwise we just use it to signal what type of endpoint it is.
                    iso = 0,
                    bulk = 1,
                    interrupt = 2,
                };

                static constexpr auto type = types::usb_ctrl_data;
                u8 target_strand_id;
                u8 target_loom_id : event::loom_bits;
                u8 target_mesh_id : event::mesh_bits;

                u8 seqnum;

                u8 ep : 4;
                ep_info_t ep_info : 2;
                bool stall : 1;
                direction_t ep_direction : 1;

                // So these are the first few bytes, the rest are in the extensions. The size is set
                // directly on the event (sizeof(data) = event.extension_bytes() - 5), not in this payload.
                u8 data[4];

                struct ext_extra_data
                {
                    u8 data[24];
                }; static_assert(sizeof(ext_extra_data) <= sizeof(fabric::event));
            }; static_assert(sizeof(usb_ctrl_data) <= sizeof(fabric::event::payload));

            struct usb_ctrl_setup_request
            {
                static constexpr auto type = types::usb_ctrl_setup_request;
                u8 target_strand_id;
                u8 target_loom_id : event::loom_bits;
                u8 target_mesh_id : event::mesh_bits;

                u8 bmRequestType;
                u8 bRequest;
                u16 wValue;
                u16 wIndex;
                // Wlength can be extracted from event.extension_bytes().
                struct ext_extra_data
                {
                    u8 data[24];
                }; static_assert(sizeof(ext_extra_data) <= sizeof(fabric::event));
            }; static_assert(sizeof(usb_ctrl_setup_request) <= sizeof(fabric::event::payload));

            struct usb_ctrl_interface_status
            {
                enum status_t : u8 { configured = 0, suspended = 1, idle = 2 };

                static constexpr auto type = types::usb_ctrl_interface_status;
                u8 target_strand_id;
                u8 target_loom_id : event::loom_bits;
                u8 target_mesh_id : event::mesh_bits;

                u8 interface_number;
                status_t status : 2;

                u8 seqnum;
            }; static_assert(sizeof(usb_ctrl_interface_status) <= sizeof(fabric::event::payload));

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

            struct request_manager_resolve
            {
                static constexpr auto type = types::request_manager_resolve;
                u32 name_hash_or_id;
                u8 seqnum;
            }; static_assert(sizeof(strand_signal) <= sizeof(fabric::event::payload));

            struct response_manager_resolve
            {
                static constexpr auto type = types::response_manager_resolve;
                u32 name_hash_or_id;
                u8 seqnum;
                u8 requester_id;
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
                u16 safe_timeout_ms; // What's a safe timeout for this manager to ensure it gets any events we throw at it.
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
                    u16 core_affinity = no_affinity;
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
                bool set_running_state = false;
                bool new_running_state = false;
            }; static_assert(sizeof(request_strand_running) <= sizeof(fabric::event::payload));

            struct response_strand_running
            {
                static constexpr auto type = types::response_strand_running;

                u8 requester_id;
                u8 seqnum;

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
    namespace framework_topic_versions::v0 { static constexpr auto topic = topic::framework; }
}
