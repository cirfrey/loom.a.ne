#pragma once

#include "lm/aliases.hpp"

#include "lm/task/types.hpp"

#include <initializer_list>

namespace lm::task { struct queue_t; } // Forward decl.

namespace lm::bus
{
    auto init() -> void;

    // You can have up to 64 event types for each topic.
    // The types themselves are defined within each module (HID events are probably in HID.hpp, etc...)
    enum topic_t : u8 {
        any = 0,

        task_status,
        task_command,

        blink,
        usbd,
        busmon,

        max_topics // Helper to check limit
    };
    enum class type_t : u8 { any = 0 };

    struct event
    {
        void* data                 = nullptr; // Feel free to recast this into whatever you want, the
                                              // value(s) here are dependent on the event type.
        lm::task::id_t sender_id = lm::task::id_t::unspecified;
        topic_t topic              = topic_t::any;
        u8 type                    = (u8)type_t::any;
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
    auto subscribe(lm::task::queue_t& queue, lm::bus::topic_t topic, std::initializer_list<u8> types = {}) -> subscribe_token;
    auto update_subscription(lm::task::queue_t& old_q, lm::task::queue_t& new_q) -> void;

    // Returns how many listeners got this event.
    auto publish(event const&) -> u32;
    //void publish_from_isr(const Event&);
}
