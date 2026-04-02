#include "lm/fabric/bus.hpp"

#include "lm/fabric/primitives.hpp"
#include "lm/utils/guarded.hpp"
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

        auto set_types(std::initializer_list<u8> ids) -> void
        {
            type_mask.reset();
            for(auto id: ids) type_mask.set(id);
        }
        auto wants_type(u8 id) const -> bool { return type_mask.test(id); }
    };

    static guarded<
        std::array<subscriber, lm::config::bus::max_subscribers>
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

auto lm::fabric::bus::subscribe(queue_t& queue, st topic, std::initializer_list<u8> types) -> subscribe_token
{
    void* subscriber = nullptr;
    bus_subs.write([&](auto& bus_subs){
        for (auto& sub : bus_subs) {
            if (sub.queue != nullptr) continue;

            sub.queue = &queue;
            sub.topic = (u8)topic;
            sub.set_types(types);
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

#include "lm/log.hpp"
#include "lm/core.hpp"
auto lm::fabric::bus::publish(event const& e) -> publish_result
{
    auto res = publish_result{};

    // NOTE: We take the mutex to iterate the list safely,
    //       but we do NOT block on queue->send.
    bus_subs.read([&](auto const& bus_subs){
        for (auto const& sub : bus_subs) {

            if (!(sub.topic == fabric::topic::any || sub.topic == e.topic))
                ++res.ignored;
            else if(!(sub.wants_type(e.type)))
                ++res.filtered;
            else if(sub.queue == nullptr || !sub.queue->send(&e, 0))
                ++res.dropped;
            else
                ++res.published;
        }
    });

    return res;
}

// ... publish_from_isr is similar but skips Mutex ...
