#include "lm/task.hpp"

#include "lm/tasks/logging.hpp"

#include "lm/board.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <utility>

// For most cases we want to publish that the task was created, but for
auto lm::task::create(lm::task::config const& cfg, task_function_t task, bool do_publish) -> void*
{
    // Used for trampolining the cfg into the task itself.
    struct task_context_t {
        task_function_t func;
        lm::task::config const* cfg;
    };
    auto* ctx = new task_context_t{
        .func = task,
        .cfg = &cfg
    };

    auto core_id = lm::board::get_physical_core(cfg.core_affinity);
    if(core_id == -1) core_id = tskNO_AFFINITY;

    TaskHandle_t handle;
    auto status = xTaskCreatePinnedToCore(
        [](void* ctx){
            auto [func, cfg] = *(task_context_t*)ctx;
            delete (task_context_t*)ctx;
            func(*cfg);
            lm::task::event::task_status::wait_for_shutdown(cfg->id);
        },
        cfg.name,
        cfg.stack_size,
        ctx,
        cfg.priority,
        &handle,
        core_id
    );
    if (status != pdPASS) {
        delete ctx; // Clean up if creation failed
        LOOM_TRACE("Spawn task [%s]...ERROR: %i\n", cfg.name, status);
        return nullptr;
    }

    if(do_publish) {
        lm::bus::publish({
            .data = (void*)handle,
            .sender_id = cfg.id,
            .topic = lm::bus::task_status,
            .type = (u8)lm::task::event::task_status::created,
        });
    }

    return handle;
}

auto lm::task::get_handle() -> void* { return xTaskGetCurrentTaskHandle(); }

auto lm::task::reap(void* handle) -> void { vTaskDelete((TaskHandle_t)handle); }

auto lm::task::delay_ms(unsigned long ms) -> void
{
    if(ms == 0) portYIELD();
    else { vTaskDelay(pdMS_TO_TICKS(ms)); }
}

// --- Queue ---
lm::task::queue_t::queue_t(u32 max_elements, u32 element_size_in_bytes)
    : impl { xQueueCreate(max_elements, element_size_in_bytes) }
    , element_size_in_bytes{ element_size_in_bytes }
    , max_elements{ max_elements }
{}

lm::task::queue_t::queue_t(queue_t&& o) { *this = static_cast<queue_t&&>(o); }
auto lm::task::queue_t::operator=(queue_t&& o) -> queue_t&
{
    this->~queue_t();
    impl = o.impl;
    o.impl = nullptr;
    element_size_in_bytes = o.element_size_in_bytes;
    o.element_size_in_bytes = 0;
    max_elements = o.max_elements;
    o.max_elements = 0;
    return *this;
}

lm::task::queue_t::~queue_t()
{
    if(impl) vQueueDelete((QueueHandle_t)impl);
}

#include "lm/tasks/logging.hpp"
auto lm::task::queue_t::send(void const* item, u32 timeout) -> bool { return xQueueSend((QueueHandle_t)impl, item, timeout) == pdTRUE; }
auto lm::task::queue_t::receive(void* into, u32 timeout) -> bool    { return xQueueReceive((QueueHandle_t)impl, into, timeout) == pdTRUE; }
auto lm::task::queue_t::free_spaces_available() const -> u32        { return uxQueueSpacesAvailable((QueueHandle_t)impl); }
auto lm::task::queue_t::capacity() const -> u32 { return max_elements; }
auto lm::task::queue_t::element_size() const -> u32 { return element_size_in_bytes; }

// --- Event Bus ---
lm::task::event_bus_t::event_bus_t(event_bus_t&& o) { *this = static_cast<event_bus_t&&>(o); }
auto lm::task::event_bus_t::operator=(event_bus_t&& o) -> event_bus_t&
{
    std::swap(this->q, o.q);
    std::swap(this->tok, o.tok);

    if (this->tok.valid()) { lm::bus::update_subscription(o.q, this->q); }

    return *this;
}

auto lm::task::event_bus_t::iterator::operator*() -> lm::bus::event& { return current_event; }
auto lm::task::event_bus_t::iterator::operator++() -> iterator&
{
    if (!bus || !bus->q.receive(&current_event, 0)) { is_done = true; }
    return *this;
}
auto lm::task::event_bus_t::iterator::operator!=(const iterator& o) const -> bool { return is_done != o.is_done; }
auto lm::task::event_bus_t::begin() -> iterator { return iterator{this, {}, false}.operator++(); }
auto lm::task::event_bus_t::end()   -> iterator { return {nullptr, {}, true}; }

auto lm::task::event_bus(u32 max_events, lm::bus::topic_t topic, std::initializer_list<u8> types) -> event_bus_t
{
    auto ret = event_bus_t();
    ret.q    = queue<lm::bus::event>(max_events);
    ret.tok  = lm::bus::subscribe(ret.q, topic, types);
    return ret;
}

auto lm::task::event::task_status::wait_for_shutdown(lm::task::id_t taskid) -> void
{
    while(1) {
        lm::bus::publish({
            .data = (void*)xTaskGetCurrentTaskHandle(),
            .sender_id = taskid,
            .topic = lm::bus::task_status,
            .type = lm::task::event::task_status::ready_for_shutdown
        });
        lm::task::delay_ms(1000);
    }
}

auto lm::task::event::task_command::make_bus(u32 size) -> event_bus_t { return lm::task::event_bus(size, lm::bus::task_command); }

auto lm::task::event::task_command::wait_for_start(event_bus_t& bus, lm::task::id_t taskid) -> void
{
    using tc = lm::task::event::task_command;
    using ts = lm::task::event::task_status;

    void* handle = xTaskGetCurrentTaskHandle();
    bool ready_to_start = false;
    while(!ready_to_start) {
        lm::bus::publish({
            .data      = handle,
            .sender_id = taskid,
            .topic     = lm::bus::task_status,
            .type      = (u8)ts::ready
        });

        for(auto const& e : bus) {
            if(e.type == tc::start && (uintptr_t)e.data == (uintptr_t)taskid) {
                ready_to_start = true;
                break;
            }
        }
        if(!ready_to_start) lm::task::delay_ms(10);
    }
}

auto lm::task::event::task_command::should_stop(event_bus_t& bus, lm::task::id_t taskid) -> bool
{
    using tc = lm::task::event::task_command;
    using ts = lm::task::event::task_status;

    for(auto const& e : bus)
        if(e.type == tc::stop && ((u8*)e.data)[0] == (u8)taskid)
            return true;

    lm::bus::publish({
        .data      = (void*)xTaskGetCurrentTaskHandle(),
        .sender_id = taskid,
        .topic     = lm::bus::task_status,
        .type      = (u8)ts::running
    });
    return false;
}
