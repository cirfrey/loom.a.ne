#include "lm/bus.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"

#include <array>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace lm::bus
{
    struct subscriber
    {
        lm::task::queue_t* queue = nullptr;
        u8 topic                 = 0;
        u64 filter_mask          = 0; // Bitmask for fast filtering
    };

    static std::array<subscriber, lm::config::bus::max_subscribers> _subs;
    static SemaphoreHandle_t _mutex = nullptr;

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
    _mutex = xSemaphoreCreateMutex();
    // Clear array
    for(auto& s : _subs) s.queue = nullptr;
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
    xSemaphoreTake(_mutex, portMAX_DELAY);

    void* subscriber = nullptr;
    for (auto& sub : _subs) {
        if (sub.queue == nullptr) {
            sub.queue = &queue;
            sub.filter_mask = make_mask(types);
            sub.topic = (u8)topic;
            subscriber = &sub;
            break;
        }
    }

    xSemaphoreGive(_mutex);
    return subscribe_token(subscriber);
}

auto lm::bus::update_subscription(lm::task::queue_t& old_q, lm::task::queue_t& new_q) -> void
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    for (auto& sub : _subs) {
        if (sub.queue == (void*)&old_q) {
            sub.queue = &new_q;
            break;
        }
    }
    xSemaphoreGive(_mutex);
}

auto lm::bus::unsubscribe(void* subscriber) -> bool
{
    xSemaphoreTake(_mutex, portMAX_DELAY);
    for (auto& sub : _subs) {
        if (&sub == subscriber) {
            sub.queue = nullptr;
            xSemaphoreGive(_mutex);
            return true;
        }
    }
    xSemaphoreGive(_mutex);
    return false;
}

auto lm::bus::publish(event const& e) -> u32
{
    u64 event_bit = (1ULL << static_cast<u8>(e.type));

    // NOTE: We take the mutex to iterate the list safely,
    //       but we do NOT block on queue->send.
    auto listeners = 0_u32;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    for (auto const& sub : _subs) {
        // Only send if the subscriber explicitly asked for this event and topic.
        if (
            sub.queue != nullptr &&
            (sub.topic       == (u8)topic_t::any || sub.topic == (u8)e.topic) &&
            (sub.filter_mask == (u8)type_t::any  || sub.filter_mask & event_bit)
        ){
            if(sub.queue->send(&e, 0)) ++listeners;
        }
    }

    xSemaphoreGive(_mutex);
    return listeners;
}

// ... publish_from_isr is similar but skips Mutex ...
