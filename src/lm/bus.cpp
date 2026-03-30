#include "lm/bus.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"
#include "lm/core/guarded.hpp"

#include <array>


namespace lm::bus
{
    struct subscriber
    {
        lm::task::queue_t* queue = nullptr;
        u8 topic                 = 0;
        u64 filter_mask          = 0; // Bitmask for fast filtering
    };

    static guarded<
        std::array<subscriber, lm::config::bus::max_subscribers>
    > bus_subs;

    // Helper: Convert List -> Bitmask
    static auto make_mask(std::initializer_list<u8> ids) -> u64
    {
        u64 m = 0;
        for (auto id : ids) {
            u8 bit = static_cast<u8>(id);
            if (bit < 64) m |= (1ULL << bit);
        }
        return m;
    }

    auto unsubscribe(void* queue) -> bool;
}

auto lm::bus::init() -> void
{
    bus_subs.write([](auto& bus_subs){
        for(auto& s : bus_subs) s.queue = nullptr;
    });
}


auto lm::bus::subscribe_token::valid() const -> bool { return impl != nullptr; }
lm::bus::subscribe_token::subscribe_token(void* subscriber) { impl = subscriber; }
lm::bus::subscribe_token::subscribe_token(subscribe_token&& o) { *this = static_cast<subscribe_token&&>(o); }
auto lm::bus::subscribe_token::operator=(subscribe_token&& o) -> subscribe_token&
{
    this->~subscribe_token();
    impl = o.impl;
    o.impl = nullptr;
    return *this;
}
lm::bus::subscribe_token::~subscribe_token() { if(impl) lm::bus::unsubscribe(impl); };

auto lm::bus::subscribe(lm::task::queue_t& queue, lm::bus::topic_t topic, std::initializer_list<u8> types) -> subscribe_token
{
    void* subscriber = nullptr;
    bus_subs.write([&](auto& bus_subs){
        for (auto& sub : bus_subs) {
            if (sub.queue != nullptr) continue;

            sub.queue = &queue;
            sub.filter_mask = make_mask(types);
            sub.topic = (u8)topic;
            subscriber = &sub;
            return;
        }

    });

    return subscribe_token(subscriber);
}

auto lm::bus::update_subscription(lm::task::queue_t& old_q, lm::task::queue_t& new_q) -> void
{
    bus_subs.write([&](auto& bus_subs){
        for (auto& sub : bus_subs) {
            if (sub.queue != (void*)&old_q) continue;

            sub.queue = &new_q;
            return;
        }
    });
}

auto lm::bus::unsubscribe(void* subscriber) -> bool
{
    bool unsubscribed = false;
    bus_subs.write([&](auto& bus_subs){
        for (auto& sub : bus_subs) {
            if (&sub != subscriber) continue;

            sub.queue = nullptr;
            unsubscribed = true;
            return;
        }
    });
    return unsubscribed;
}

auto lm::bus::publish(event const& e) -> u32
{
    u64 event_bit = (1ULL << static_cast<u8>(e.type));

    // NOTE: We take the mutex to iterate the list safely,
    //       but we do NOT block on queue->send.
    auto listeners = 0_u32;
    bus_subs.read([&](auto const& bus_subs){
        for (auto const& sub : bus_subs) {
            // Only send if the subscriber explicitly asked for this event and topic.
            if (
                sub.queue != nullptr &&
                (sub.topic       == (u8)topic_t::any || sub.topic == (u8)e.topic) &&
                (sub.filter_mask == (u8)type_t::any  || sub.filter_mask & event_bit)
            ){
                if(sub.queue->send(&e, 0)) ++listeners;
            }
        }
    });
    return listeners;
}

// ... publish_from_isr is similar but skips Mutex ...
