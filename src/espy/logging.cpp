#include "espy/logging.hpp"

#include "espy/bus.hpp"
#include "espy/usb/usb.hpp"
#include "espy/usb/hid.hpp"
#include "espy/config.hpp"
#include "espy/task.hpp"
#include "espy/utils/math.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"

#include <optional>
#include <cstdarg>
#include <bitset>

#include "esp_log.h"

#include "tusb.h"

namespace espy::logging
{
    // Internal handle for the memory pipe
    static RingbufHandle_t log_ringbuf = nullptr;

    auto task(void*) -> void;

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

auto espy::logging::logf(const char *fmt, ...) -> int
{
    if (!log_ringbuf) { return 0; } // Logging not initialized or there was an error during init.

    auto const bufsize = espy::config::logging::logf_bufsize;
    char buf[bufsize];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, bufsize, fmt, args);
    va_end(args);

    if (len <= 0) return len;

    // The data we send does not have the null terminator, to send that
    // we'd have to do len+1 here.
    // We'd rather just treat the ringbuf as a byte stream and stream
    // the bytes from that to the backends, instead of dealing with individual
    // chunks of text.
    if (xRingbufferSend(log_ringbuf, buf, len, 0) != pdTRUE) {
        /// TODO: Post a "dropped_log" message to the bus.
        return 0;
    }

    return len;
}

auto espy::logging::init() -> void
{
    espy::logging::log_ringbuf = xRingbufferCreate(espy::config::logging::ringbuf_size, RINGBUF_TYPE_BYTEBUF);

    if (!log_ringbuf) {
        // Fatal error: No RAM. Blink LED or panic here.
        /// TODO: post to event bus
        return;
    }

    espy::task::create(espy::config::task::logging, espy::logging::task);
}

auto espy::logging::task(void*) -> void
{
    constexpr auto& cfg = espy::config::task::logging;

    auto usb_event_bus = espy::task::event_bus(cfg.bus_size, espy::bus::usb);

    constexpr auto hid_dispatcher_id = 0_u8;
    constexpr auto cdc_dispatcher_id = 1_u8;
    std::optional<dispatcher> hid_dispatcher = std::nullopt;
    std::optional<dispatcher> cdc_dispatcher = std::nullopt;
    constexpr auto max_dispatchers = 2;
    std::bitset<max_dispatchers> dispatchers_enabled = {0};
    static auto dispatchbuf = RingBuf<
        dispatch_data_t< max_dispatchers >,
        espy::config::logging::dispatchbuf_max_size
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
        dispatchers_enabled.set(id, true);
        d = dispatcher{ .id = id, .cb = cb };
    };
    auto disable_dispatcher = [&](auto& d) -> void {
        if(!d) return;
        dispatchers_enabled.set(d->id, false);
        d = std::nullopt;
    };

    while (1)
    {
        // Consume events.
        for(auto& e : usb_event_bus) { switch((espy::usb::event)e.type){
            case espy::usb::event::cdc_enabled:  enable_dispatcher(cdc_dispatcher, cdc_dispatcher_id, cdc_dispatcher_callback); break;
            case espy::usb::event::hid_enabled:  enable_dispatcher(hid_dispatcher, hid_dispatcher_id, hid_dispatcher_callback); break;
            case espy::usb::event::cdc_disabled: disable_dispatcher(cdc_dispatcher); break;
            case espy::usb::event::hid_disabled: disable_dispatcher(hid_dispatcher); break;
            default: break;
        }}

        if(dispatchbuf.empty() && !try_get_bytes(cfg.sleep_ms)) continue;

        if(cdc_dispatcher) cdc_dispatcher->dispatch(dispatchbuf);
        if(hid_dispatcher) hid_dispatcher->dispatch(dispatchbuf);

        // Clear all the data that's been dispatched by all the dispatchers already.
        while(dispatchbuf.size() && (dispatchbuf[0].dispatchers_done & dispatchers_enabled).count() == dispatchers_enabled.count()) {
            vRingbufferReturnItem(log_ringbuf, dispatchbuf[0].data);
            dispatchbuf.dequeue();
        }

        // Throttling so we don't starve anyone else.
        espy::task::delay_ms(cfg.sleep_ms);

        // Try to clear data from the ringbuf while we're at it.
        while(try_get_bytes(0));
    }
}

template <typename Buf>
auto espy::logging::dispatcher::dispatch(Buf& buf) -> void
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

auto espy::logging::hid_dispatcher_callback(u8* data, u32 data_size, dispatcher::transmission_info_t& ti) -> dispatcher::status_t
{
    if(ti.failed_tries == 5) { ti.bytes_sent = data_size; } // If failed too many times just mark it as done.

    if(!tud_hid_ready()) { return dispatcher::status_t::fail; }

    constexpr auto max_chunk_size = 63;
    u8 report[64] = {0};

    int chunk_size = espy::math::clamp(data_size - ti.bytes_sent, 0, max_chunk_size);
    memcpy(report, data + ti.bytes_sent, chunk_size);
    if(tud_hid_report((u8)espy::usb::hid::hid_reportid::vendor, report, 63)) {
        ti.bytes_sent += chunk_size;
        ti.failed_tries = 0; // Reset failure count on success
        return dispatcher::status_t::backoff;
    }

    return dispatcher::status_t::fail;
}

auto espy::logging::cdc_dispatcher_callback(u8* data, u32 data_size, dispatcher::transmission_info_t& ti) -> dispatcher::status_t
{
    if(!tud_cdc_connected()) { ti.bytes_sent = data_size; } // Skip if CDC is not connected.

    uint32_t avail = tud_cdc_write_available();
    if(avail == 0) { return dispatcher::status_t::backoff; }

    uint32_t chunk = espy::math::clamp(data_size - ti.bytes_sent, 0, avail);
    auto sent = tud_cdc_write(data + ti.bytes_sent, chunk);
    ti.bytes_sent += sent;

    if(sent != chunk)     return dispatcher::status_t::fail;
    //else if(sent < avail) return dispatcher::status_t::ready_for_more;
    /*else*/              return dispatcher::status_t::backoff;
}
