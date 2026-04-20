#pragma once

#include "lm/core/types.hpp"

#include "lm/fabric/types.hpp"
#include "lm/fabric/primitives.hpp"

#include <initializer_list>
#include <span>

namespace lm::fabric::bus
{
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

    enum userfilter_return
    {
        pass,
        filter,
    };
    using userfilter_t = userfilter_return(*)(void* userdata, std::span<fabric::event> events);
    // Pass a list of IDs you care about: subscribe(q, topic_t::usb, {EventID::Panic, EventID::UsbConnected, ...})
    // If the list is empty it is assumed you are intered in every event type.
    // If you drop the token you unsubscribe. If you wanna keep subscribed across scopes you better move it.
    auto subscribe(
        lm::fabric::queue_t& queue,
        u8 topic,
        std::initializer_list<u8> types = {},
        userfilter_t userfilter = nullptr,
        void* userfilter_userdata = nullptr
    ) -> subscribe_token;

    struct publish_result
    {
        st published    = 0; // How many subscribers actually got this event.
        st dropped      = 0; // How many subscribers wanted to get this event but their queues were full or could receive the event for whatever reason.
        st uninterested = 0; // How many subscribers are interested in this topic but not this type.
        st filtered     = 0; // How many subscribers are interested in this topic and type but not this event specifically.
        st ignored      = 0; // How many subscribers simply aren't interested in the topic.
    };
    // Automatically fills in the timestamp (if set to zero) for you, how nice.
    auto publish(event&) -> publish_result;
    // Assumes event 1 is the header and the rest are the bodies.
    auto publish(std::span<event> extended_event) -> publish_result;
    // Same as above.
    // Just a convenience wrapper.
    template<typename... Events>
    requires (std::same_as<event, Events> && ...)
    auto publish(Events... events) -> publish_result;

    /// TODO: void publish_from_isr(const Event&);
}

// Impls.

template <typename... Events>
auto lm::fabric::bus::publish(Events... events) -> publish_result
{
    event es[] = { events... };
    return publish(es);
}
