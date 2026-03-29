#include "lm/tasks/logging.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"
#include "lm/bus.hpp"
#include "lm/board.hpp"

#include "lm/tasks/usbd.hpp"
#include "lm/usbd/hid.hpp"

#include "lm/utils/stopwatch.hpp"
#include "lm/utils/math.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/task.h>

#include <optional>
#include <cstdarg>
#include <bitset>

#include <esp_log.h>

/// TODO: ideally this wouldn't use tinyusb directly.
///       Maybe we should expose a couple usb functions on usbd.hpp
#include <tusb.h>

/// --- Uart stuff ---

/// TODO: hardcoded pins for now.
auto lm::logging::init_uart() -> void
{
    // Forwards ESP_LOG* to uart.
    esp_log_set_vprintf([](const char* format, va_list args) {
        char buf[lm::config::logging::logf_bufsize];
        int len = vsnprintf(buf, sizeof(buf), format, args);
        if (len > 0) { lograw_uart(buf, len); }
        return len; // Return the number of characters written
    });
}

auto lm::logging::logf_uart(const char* fmt, ...) -> int
{
    auto const bufsize = lm::config::logging::logf_bufsize;
    char buf[bufsize];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, bufsize, fmt, args);
    va_end(args);

    if (len <= 0) return len;

    lograw_uart(buf, len);
    return len;
}

auto lm::logging::lograw_uart(const char* bytes, u32 len) -> void { lm::board::console_write(bytes, len); }

/// --- CDC & HID stuff ---

namespace lm::logging
{
    // Internal handle for the memory pipe
    static RingbufHandle_t log_ringbuf = nullptr;

    auto task(lm::task::config const&) -> void;

    struct dispatcher{
        struct transmission_info_t {
            u32 bytes_sent = 0;
            u32 total_tries = 0;
            u32 failed_tries = 0;
        };

        enum class status_t {
            ready_for_more,
            fail,
            backoff,
        };

        using callback = status_t(*)(u8* /*data*/, u32 /*data_size*/, transmission_info_t& /*bytes_sent*/);

        u32 id;
        callback cb;
        transmission_info_t transmission_info = {};

        auto dispatch(auto& buf) -> void;
    };

    auto hid_dispatcher_callback(u8* data, u32 data_size, dispatcher::transmission_info_t& ti) -> dispatcher::status_t;
    auto cdc_dispatcher_callback(u8* data, u32 data_size, dispatcher::transmission_info_t& ti) -> dispatcher::status_t;

    template <u16 MaxDispatchers>
    struct dispatch_data_t
    {
        size_t size;
        u8* data;

        static constexpr auto max_dispatchers = MaxDispatchers;
        std::bitset<max_dispatchers> dispatchers_done = {0};
    };

    // Very simple ringbuf with basically no safety or concurrency stuff, just
    // to be used in this file in this thread.
    template <typename T, std::size_t Capacity>
    struct RingBuf
    {
        using value_t = T;
        static constexpr auto capacity = Capacity;

        std::array<T, Capacity> data;
        size_t head = 0;   // Index of the front element
        size_t tail = 0;   // Index where the next element will be inserted
        size_t count = 0;  // Current number of elements

        bool empty() const { return count == 0; }
        bool full() const { return count == Capacity; }
        size_t size() const { return count; }

        void enqueue(const T& value) {
            data[tail] = value;
            tail = (tail + 1) % Capacity; // Wrap around
            count++;
        }

        void dequeue() {
            head = (head + 1) % Capacity; // Wrap around
            count--;
        }

        auto operator[](u32 idx) -> T& { return data[(head + idx) % Capacity]; }
    };
}

auto lm::logging::logf(const char* fmt, ...) -> int
{
    if (!log_ringbuf) { return 0; } // Logging not initialized or there was an error during init.

    auto const bufsize = lm::config::logging::logf_bufsize;
    char buf[bufsize];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, bufsize, fmt, args);
    va_end(args);

    if (len <= 0) return len;

    lm::logging::lograw(buf, len);
    return len;
}

auto lm::logging::lograw(const char* bytes, u32 len) -> void
{
    const u32 max_preview = 64;
    const u32 uart_limit = (len < max_preview) ? len : max_preview;

    u32 nl_idx = 0;
    bool found_nl = false;

    for (; nl_idx < uart_limit; ++nl_idx) {
        if (bytes[nl_idx] == '\n') {
            found_nl = true;
            break;
        }
    }

    const char* status = xRingbufferSend(log_ringbuf, bytes, len, 0) == pdTRUE ? "" : "[WARN(dropped)] ";
    const char* logtype;
    u32 msgsize;
    if (len <= max_preview && !found_nl){
        logtype = "full";
        msgsize = len;
    } else if (len <= max_preview && found_nl && nl_idx == len - 1) {
        logtype = "f-nl";
        msgsize = nl_idx;
    } else if (found_nl) {
        logtype = "t-nl";
        msgsize = nl_idx;
    } else {
        logtype = "trun";
        msgsize = max_preview;
    }
    LOOM_TRACE("%slog(%3u)|%s: %.*s\n", status, len, logtype, msgsize, bytes);
}

auto lm::logging::init() -> void
{
    lm::logging::log_ringbuf = xRingbufferCreate(lm::config::logging::ringbuf_size, RINGBUF_TYPE_BYTEBUF);

    if (!log_ringbuf) {
        LOOM_TRACE("Failed to init logging\n");
        // Fatal error: No RAM. Blink LED or panic here.
        /// TODO: post to event bus
        return;
    }

    lm::task::create(lm::config::task::logging, lm::logging::task);
}

auto lm::logging::task(lm::task::config const& cfg) -> void
{
    using tc = lm::task::event::task_command;

    constexpr auto hid_dispatcher_id = 0_u8;
    constexpr auto cdc_dispatcher_id = 1_u8;
    std::optional<dispatcher> hid_dispatcher = std::nullopt;
    std::optional<dispatcher> cdc_dispatcher = std::nullopt;
    constexpr auto max_dispatchers = 2;
    std::bitset<max_dispatchers> dispatchers_enabled = {0};
    static auto dispatchbuf = RingBuf<
        dispatch_data_t< max_dispatchers >,
        lm::config::logging::dispatchbuf_max_size
    >{};

    auto try_get_bytes = [&](auto delay) -> bool {
        if(dispatchbuf.full()) return false;

        size_t size;
        u8* data = (u8*)xRingbufferReceive(log_ringbuf, &size, delay);
        if(data == nullptr) return false;

        dispatchbuf.enqueue({ .size = size, .data = data });
        return true;
    };

    auto enable_dispatcher = [&](auto& d, auto id, auto cb) -> void {
        if(d) return;
        LOOM_TRACE("Enabled dispatcher %i\n", id);
        dispatchers_enabled.set(id, true);
        d = dispatcher{ .id = id, .cb = cb };
    };
    auto disable_dispatcher = [&](auto& d) -> void {
        if(!d) return;
        LOOM_TRACE("Disabled dispatcher %i\n", d->id);
        dispatchers_enabled.set(d->id, false);
        d = std::nullopt;
    };

    // Every once in a bit we should query the usbd task to figure out if the dispatchers
    // are appropriate, this is the timer for that.
    auto usbd_status_timer = lm::simple_timer<std::chrono::seconds>(10); // Every 10s.

    auto usbd_bus = lm::task::event_bus(8, lm::bus::usbd);
    auto tc_bus = tc::make_bus();
    tc::wait_for_start(tc_bus, cfg.id);

    usbd_status_timer.start();
    while (!tc::should_stop(tc_bus, cfg.id))
    {
        if(usbd_status_timer.is_done()) {
            usbd_status_timer.restart();
            lm::bus::publish({
                .sender_id = cfg.id,
                .topic = lm::bus::usbd,
                .type = (u8)lm::usbd::event::get_status,
            });
        }

        // Consume usbd events.
        for(auto& e : usbd_bus) { switch((lm::usbd::event)e.type){
            case lm::usbd::event::cdc_enabled:  enable_dispatcher(cdc_dispatcher, cdc_dispatcher_id, cdc_dispatcher_callback); break;
            case lm::usbd::event::hid_enabled:  enable_dispatcher(hid_dispatcher, hid_dispatcher_id, hid_dispatcher_callback); break;
            case lm::usbd::event::cdc_disabled: disable_dispatcher(cdc_dispatcher); break;
            case lm::usbd::event::hid_disabled: disable_dispatcher(hid_dispatcher); break;
            default: break;
        }}

        if(dispatchbuf.empty() && !try_get_bytes(cfg.sleep_ms)) continue;

        if(cdc_dispatcher) {
            cdc_dispatcher->dispatch(dispatchbuf);
            tud_cdc_write_flush();
        }
        if(hid_dispatcher) hid_dispatcher->dispatch(dispatchbuf);

        // Clear all the data that's been dispatched by all the dispatchers already.
        while(dispatchbuf.size() && (dispatchbuf[0].dispatchers_done & dispatchers_enabled).count() == dispatchers_enabled.count()) {
            vRingbufferReturnItem(log_ringbuf, dispatchbuf[0].data);
            dispatchbuf.dequeue();
        }

        // Throttling so we don't starve anyone else.
        lm::task::delay_ms(cfg.sleep_ms);

        // Try to clear data from the ringbuf while we're at it.
        while(try_get_bytes(0));
    }
}

template <typename Buf>
auto lm::logging::dispatcher::dispatch(Buf& buf) -> void
{
    auto status = status_t::ready_for_more;

    while(status == status_t::ready_for_more)
    {
        // Find first dispatchable not yet fully dispatched by this dispatcher.
        typename Buf::value_t* not_dispatched = nullptr;
        for(auto i = 0; i < buf.size(); ++i)
        if(buf[i].dispatchers_done[id] == false) {
            not_dispatched = &buf[i];
            break;
        }
        if(not_dispatched == nullptr) return; // Nothing to dispatch.

        status = cb(not_dispatched->data, not_dispatched->size, transmission_info);
        ++transmission_info.total_tries;
        transmission_info.failed_tries += (status == status_t::fail);

        // Dispatch done.
        if(transmission_info.bytes_sent >= not_dispatched->size) {
            not_dispatched->dispatchers_done.set(id, true);
            transmission_info = {};
        }
    }
}

auto lm::logging::hid_dispatcher_callback(u8* data, u32 data_size, dispatcher::transmission_info_t& ti) -> dispatcher::status_t
{
    if(ti.failed_tries == 5) { ti.bytes_sent = data_size; return dispatcher::status_t::fail; } // If failed too many times just mark it as done.

    if(!tud_hid_ready()) { return dispatcher::status_t::backoff; }

    constexpr auto max_chunk_size = 63;
    u8 report[64] = {0};

    int chunk_size = lm::math::clamp(data_size - ti.bytes_sent, 0, max_chunk_size);
    memcpy(report, data + ti.bytes_sent, chunk_size);
    if(tud_hid_report((u8)lm::usbd::hid::hid_reportid::vendor, report, max_chunk_size)) {
        ti.bytes_sent += chunk_size;
        ti.failed_tries = 0; // Reset failure count on success
        return dispatcher::status_t::backoff;
    }

    return dispatcher::status_t::fail;
}

auto lm::logging::cdc_dispatcher_callback(u8* data, u32 data_size, dispatcher::transmission_info_t& ti) -> dispatcher::status_t
{
    if(!tud_cdc_connected()) { ti.bytes_sent = data_size; return dispatcher::status_t::fail; } // Skip if CDC is not connected.

    uint32_t avail = tud_cdc_write_available();
    if(avail == 0) { return dispatcher::status_t::backoff; }

    uint32_t chunk = lm::math::clamp(data_size - ti.bytes_sent, 0, avail);
    auto sent = tud_cdc_write(data + ti.bytes_sent, chunk);
    ti.bytes_sent += sent;

    if(sent != chunk)     return dispatcher::status_t::fail;
    else if(sent < avail) return dispatcher::status_t::ready_for_more;
    /*else*/              return dispatcher::status_t::backoff;
}
