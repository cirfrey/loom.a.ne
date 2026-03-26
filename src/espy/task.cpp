#include "espy/task.hpp"

#include "espy/config.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "esp_chip_info.h"

auto espy::task::create(espy::config::task::info_t const& info, task_function_t task, void* params) -> void*
{
    int affinity = 0;

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    using aff_t = espy::config::task::info_t::affinity_t;
    switch(info.core_affinity){
        case aff_t::no_affinity:     { affinity = tskNO_AFFINITY; break; }
        case aff_t::core_processing: { affinity = chip_info.model == CHIP_ESP32S2 ? 0 : 1; break; }
        case aff_t::core_wifi:       { affinity = chip_info.model == CHIP_ESP32S2 ? 0 : 0; break; }
        default:                     { affinity = 0; }
    }

    TaskHandle_t handle;
    xTaskCreatePinnedToCore(
        task,
        info.name,
        info.stack_size,
        params,
        info.priority,
        &handle, /// TODO: broadcast a TASK_CREATED event with this handle.
        affinity
    );
    return handle;
}

auto espy::task::delay_ms(unsigned long ms) -> void
{
    if(ms == 0) portYIELD();
    else { vTaskDelay(pdMS_TO_TICKS(ms)); }
}

// --- Queue ---
espy::task::queue_t::queue_t(u32 max_elements, u32 element_size_in_bytes)
    : impl { xQueueCreate(max_elements, element_size_in_bytes) }
{}

espy::task::queue_t::queue_t(queue_t&& o) { *this = static_cast<queue_t&&>(o); }
auto espy::task::queue_t::operator=(queue_t&& o) -> queue_t&
{
    this->~queue_t();
    impl = o.impl;
    o.impl = nullptr;
    return *this;
}

espy::task::queue_t::~queue_t()
{
    if(impl) vQueueDelete((QueueHandle_t)impl);
}

auto espy::task::queue_t::send(void const* item, u32 timeout) -> void { xQueueSend((QueueHandle_t)impl, item, timeout); }
auto espy::task::queue_t::receive(void* into, u32 timeout) -> bool    { return xQueueReceive((QueueHandle_t)impl, into, timeout) == pdTRUE; }

// --- Event Bus ---
espy::task::event_bus_t::event_bus_t(event_bus_t&& o) { *this = static_cast<event_bus_t&&>(o); }
auto espy::task::event_bus_t::operator=(event_bus_t&& o) -> event_bus_t&
{
    this->~event_bus_t();
    q   = static_cast<queue_t&&>(o.q);
    tok = static_cast<espy::bus::subscribe_token&&>(o.tok);
    return *this;
}

auto espy::task::event_bus_t::iterator::operator*() -> espy::bus::event& { return current_event; }
auto espy::task::event_bus_t::iterator::operator++() -> iterator&
{
    if (!bus || !bus->q.receive(&current_event, 0)) { is_done = true; }
    return *this;
}
auto espy::task::event_bus_t::iterator::operator!=(const iterator& o) const -> bool { return is_done != o.is_done; }
auto espy::task::event_bus_t::begin() -> iterator { return iterator{this, {}, false}.operator++(); }
auto espy::task::event_bus_t::end()   -> iterator { return {nullptr, {}, true}; }

auto espy::task::event_bus(u32 max_events, espy::bus::topic_t topic, std::initializer_list<u8> types) -> event_bus_t
{
    auto ret = event_bus_t();
    ret.q    = queue<espy::bus::event>(max_events);
    ret.tok  = espy::bus::subscribe(ret.q, topic, types);
    return ret;
}
