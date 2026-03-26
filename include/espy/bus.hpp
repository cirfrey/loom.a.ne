#pragma once

#include "espy/aliases.hpp"

#include <initializer_list>

namespace espy::task { struct queue_t; } // Forward decl.

namespace espy::bus
{
    auto init() -> void;

    // You can have up to 64 event types for each topic.
    // The types themselves are defined within each module (HID events are probably in HID.hpp, etc...)
    enum topic_t : u8 {
        any = 0,

        blink,
        usb,

        // System Critical
        SystemBoot,
        Panic,

        // USB & Logging
        HID_Enabled,
        HID_Disabled,
        CDC_Enabled,
        CDC_Disabled,

        MAX_EVENTS // Helper to check limit
    };
    enum class type_t : u8 { any = 0 };

    struct event
    {
        static constexpr auto sender_unspecified = 0;

        u8 sender_id = sender_unspecified;
        topic_t topic = topic_t::any;
        u8 type = (u8)type_t::any;
        u8 data[sizeof(void*)] = {0};
    };

    struct subscribe_token
    {
        auto valid() const -> bool;

        subscribe_token() = default;
        subscribe_token(void* q);
        subscribe_token(subscribe_token const&) = delete;
        subscribe_token(subscribe_token&& o);
        auto operator=(const subscribe_token&) -> subscribe_token& = delete;
        auto operator=(subscribe_token&& o) -> subscribe_token&;

        ~subscribe_token();

    private:
        void* impl = nullptr;
    };

    // 3. API
    // Pass a list of IDs you care about: subscribe(q, topic_t::usb, {EventID::Panic, EventID::UsbConnected, ...})
    auto subscribe(espy::task::queue_t& queue, espy::bus::topic_t topic, std::initializer_list<u8> types = {}) -> subscribe_token;

    // Returns how many listeners got this event.
    auto publish(event const&) -> u32;
    //void publish_from_isr(const Event&);
}
