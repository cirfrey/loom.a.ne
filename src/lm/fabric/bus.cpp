#include "lm/fabric/bus.hpp"

#include "lm/chip/time.hpp"

#include "lm/fabric/primitives.hpp"
#include "lm/core/primitives/guarded.hpp"
#include "lm/config.hpp"

#include <array>
#include <bitset>

namespace lm::fabric::bus
{
    struct subscriber
    {
        queue_t* queue                                   = nullptr;
        u8 topic                                         = 0;
        std::bitset<(decltype(event::type))-1> type_mask = 0;

        userfilter_t userfilter;
        void* userfilter_userdata;

        auto set_types(std::initializer_list<u8> ids) -> void
        {
            type_mask.reset();
            for(auto id: ids) type_mask.set(id);
        }
        auto wants_type(u8 id) const -> bool { return type_mask.none() || type_mask.test(id); }
    };

    static guarded<
        std::array<subscriber, config_t::bus_t::max_subscribers>
    > bus_subs;

    // Not exposed to the general public, they just get the token system.
    // It's for their own safety.
    auto unsubscribe(void* queue) -> void;
}

auto lm::fabric::bus::subscribe_token::valid() const -> bool { return impl != nullptr; }
lm::fabric::bus::subscribe_token::subscribe_token(void* subscriber) { impl = subscriber; }
lm::fabric::bus::subscribe_token::subscribe_token(subscribe_token&& o) { *this = static_cast<subscribe_token&&>(o); }
auto lm::fabric::bus::subscribe_token::operator=(subscribe_token&& o) -> subscribe_token&
{
    auto temp = impl;
    impl = o.impl;
    o.impl = temp;
    return *this;
}
lm::fabric::bus::subscribe_token::~subscribe_token() { if(impl) unsubscribe(impl); };

auto lm::fabric::bus::subscribe(
    queue_t& queue,
    u8 topic,
    std::initializer_list<u8> types,
    userfilter_t userfilter,
    void* userfilter_userdata
) -> subscribe_token
{
    void* subscriber = nullptr;
    bus_subs.write([&](auto& bus_subs){
        for (auto& sub : bus_subs) {
            if (sub.queue != nullptr) continue;

            sub.queue = &queue;
            sub.topic = (u8)topic;
            sub.set_types(types);
            sub.userfilter = userfilter;
            sub.userfilter_userdata = userfilter_userdata;
            subscriber = &sub;
            return;
        }
    });
    return subscribe_token(subscriber);
}

auto lm::fabric::bus::unsubscribe(void* subscriber) -> void
{
    bus_subs.write([&](auto& bus_subs){
        for (auto& sub : bus_subs) {
            if (&sub != subscriber) continue;

            sub.queue = nullptr;
            return;
        }
    });
}


namespace lm::fabric::bus
{
    static auto publish_impl(
        decltype(bus_subs)::value_type& bus_subs,
        publish_result* res,
        std::span<event> events
    )
    {
        if(!events.size()) return;

        auto& header = events[0];
        header.size  = events.size() * sizeof(event);
        // TODO: works for now, will break when mesh is implemented.
        header.timestamp = chip::time::uptime();

        for (auto const& sub : bus_subs) {
            if(sub.queue == nullptr) continue;

            auto const not_interested_in_topic = !(sub.topic == topic::any || sub.topic == header.topic);
            auto const not_interested_in_type  = !(sub.wants_type(header.type));

            if(not_interested_in_topic) { ++res->ignored;      continue; }
            if(not_interested_in_type)  { ++res->uninterested; continue; }

            auto const not_enough_slots = sub.queue->slots() < events.size();
            if(not_enough_slots)        { ++res->dropped;      continue; }

            if(sub.userfilter == nullptr || sub.userfilter(sub.userfilter_userdata, events) == userfilter_return::pass)
                { ++res->published; for(auto& e : events) sub.queue->send(&e, 0); }
            else
                { ++res->filtered; }
        }
    }
}

auto lm::fabric::bus::publish(event& e) -> publish_result
{
    auto res = publish_result{};

    // NOTE: We take the mutex to iterate the list safely,
    //       but we do NOT block on queue->send.
    bus_subs.write(publish_impl, &res, std::span<event>{&e, 1});

    return res;
}

auto lm::fabric::bus::publish(std::span<event> extended_event) -> publish_result
{
    auto res = publish_result{};

    // NOTE: We take the mutex to iterate the list safely,
    //       but we do NOT block on queue->send.
    bus_subs.write(publish_impl, &res, extended_event);

    return res;
}

// ... publish_from_isr is similar but skips Mutex ...
