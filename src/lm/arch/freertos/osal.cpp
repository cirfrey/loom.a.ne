/* --- primitives.cpp --- */

#include "lm/fabric/primitives.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

// --- Bytebuf ---

lm::fabric::bytebuf::bytebuf(st bytes)
    : impl{ xRingbufferCreate(bytes, RINGBUF_TYPE_BYTEBUF)}
{}

lm::fabric::bytebuf::bytebuf(bytebuf&& o)
{ *this = static_cast<bytebuf&&>(o); }

auto lm::fabric::bytebuf::operator=(bytebuf&& o) -> bytebuf&
{
    auto timpl = impl;
    impl = o.impl;
    o.impl = timpl;

    return *this;
}

lm::fabric::bytebuf::~bytebuf()
{
    if(impl) vRingbufferDelete((RingbufHandle_t)impl);
}

auto lm::fabric::bytebuf::send(buf data, u32 timeout) const -> bool
{ return xRingbufferSend((RingbufHandle_t)impl, data.data, data.size, timeout) == pdTRUE; }

auto lm::fabric::bytebuf::receive(u32 timeout) const -> buf
{
    size_t size = 0;
    u8* data = (u8*)xRingbufferReceive((RingbufHandle_t)impl, &size, timeout);
    return { .data = data, .size = size };
}

auto lm::fabric::bytebuf::free(void* data) const -> void
{
    vRingbufferReturnItem((RingbufHandle_t)impl, data);
}

auto lm::fabric::bytebuf::initialized() const -> bool
{
    return impl != nullptr;
}

// --- Queue ---
lm::fabric::queue_t::queue_t(st max_elements, st element_size_in_bytes)
    : impl { xQueueCreate(max_elements, element_size_in_bytes) }
    , element_size_in_bytes{ element_size_in_bytes }
    , max_elements{ max_elements }
{}

lm::fabric::queue_t::queue_t(queue_t&& o) { *this = static_cast<queue_t&&>(o); }
auto lm::fabric::queue_t::operator=(queue_t&& o) -> queue_t&
{
    auto timpl = impl;
    impl = o.impl;
    o.impl = timpl;

    auto tel = element_size_in_bytes;
    element_size_in_bytes = o.element_size_in_bytes;
    o.element_size_in_bytes = tel;

    auto tmel = max_elements;
    max_elements = o.max_elements;
    o.max_elements = tmel;

    return *this;
}

lm::fabric::queue_t::~queue_t()
{
    if(impl) vQueueDelete((QueueHandle_t)impl);
}

auto lm::fabric::queue_t::send(void const* item, u32 timeout) -> bool { return xQueueSend((QueueHandle_t)impl, item, timeout) == pdTRUE; }
auto lm::fabric::queue_t::receive(void* into, u32 timeout) -> bool    { return xQueueReceive((QueueHandle_t)impl, into, timeout) == pdTRUE; }

auto lm::fabric::queue_t::slots() const -> st { return uxQueueSpacesAvailable(QueueHandle_t)impl); }

auto lm::fabric::queue_t::capacity() const     -> st { return max_elements; }
auto lm::fabric::queue_t::element_size() const -> st { return element_size_in_bytes; }

auto lm::fabric::semaphore::create_binary() -> semaphore
{
    semaphore s;
    s.impl = xSemaphoreCreateBinary();
    return s;
}

auto lm::fabric::semaphore::create_counting(u32 max, u32 initial) -> semaphore
{
    semaphore s;
    s.impl = xSemaphoreCreateCounting(max, initial);
    return s;
}

lm::fabric::semaphore::semaphore(semaphore&& o) noexcept : impl(o.impl) {
    o.impl = nullptr;
}

auto lm::fabric::semaphore::operator=(semaphore&& o) noexcept -> semaphore& {
    auto timpl = impl;
    impl = o.impl;
    o.impl = timpl;
    return *this;
}

lm::fabric::semaphore::~semaphore()
{ if (impl) vSemaphoreDelete((SemaphoreHandle_t)impl); }

auto lm::fabric::semaphore::take(u32 timeout_ms) const -> bool
{
    if (!impl) return false;
    return xSemaphoreTake((SemaphoreHandle_t)impl, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

auto lm::fabric::semaphore::give() const -> void
{ if (impl) xSemaphoreGive((SemaphoreHandle_t)impl); }

auto lm::fabric::semaphore::initialized() const -> bool
{ return impl != nullptr; }

/* --- task.cpp --- */

#include "lm/fabric/task.hpp"

#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"

#include "lm/chip/system.hpp"
#include "lm/core/cvt.hpp"
#include "lm/log.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <utility>
#include <memory>

auto lm::fabric::strand::create(
    task_constants const& cfg,
    task_runtime_info const& info,
    function_t task,
    void* taskarg
) -> handle_t
{
    auto core_count = chip::system::core_count();
    auto core_id = cfg.core_affinity == task_constants::no_affinity
        ? tskNO_AFFINITY
        : cfg.core_affinity >= core_count ? core_count - 1 : cfg.core_affinity; /// TODO: this works for now but doesn't scale

    TaskHandle_t handle;
    auto status = xTaskCreatePinnedToCore(
        task,
        cfg.name,
        cfg.stack_size,
        taskarg ? taskarg : (info | smuggle<void*>),
        cfg.priority,
        &handle,
        (BaseType_t)core_id
    );
    if (status != pdPASS) {
        log::error("Spawn task [%s]...ERROR: %i\n", cfg.name, status);
        return nullptr;
    }

    bus::publish(fabric::event{
        .topic = topic::task,
        .type = event::created,
        .strand_id = info.id,
    }.with_payload<status_event>({ .handle = handle }));

    return handle;
}

auto lm::fabric::strand::get_handle() -> handle_t { return xTaskGetCurrentTaskHandle(); }

auto lm::fabric::strand::reap(handle_t handle) -> void { vTaskDelete((TaskHandle_t)handle); }

auto lm::fabric::strand::sleep_ms(unsigned long ms) -> void
{
    if(ms == 0) portYIELD();
    else { vTaskDelay(pdMS_TO_TICKS(ms)); }
}
