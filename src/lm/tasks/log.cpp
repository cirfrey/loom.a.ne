#include "lm/tasks/log.hpp"

#include "lm/utils/guarded.hpp"
#include "lm/fabric/primitives.hpp"
#include "lm/config.hpp"
#include "lm/log.hpp"
#include "lm/board.hpp"
#include "lm/core/math.hpp"

#include "lm/tasks/usbd.hpp"
#include "lm/usbd/hid.hpp"

#include "lm/utils/stopwatch.hpp"

#include <tusb.h>

namespace lm::tasks::logging
{
    static lm::guarded<fabric::bytebuf> logbuf;
}

auto lm::tasks::log::dispatch(text t) -> bool
{
    bool dispatched = false;
    logging::logbuf.read([&](auto& buf){
        if(buf.initialized()) dispatched = buf.send(t, 0);
    });
    return dispatched;
}

auto lm::tasks::log::capacity() -> st
{
    st ret = 0;
    logging::logbuf.read([&](auto& buf){
        if(buf.initialized()) ret = buf.capacity();
    });
    return ret;
}

auto lm::tasks::log::occupancy() -> st
{
    st ret = 0;
    logging::logbuf.read([&](auto& buf){
        if(buf.initialized()) ret = buf.occupancy();
    });
    return ret;
}


lm::tasks::log::log(fabric::task_runtime_info& info)
{
    //     // Forwards ESP_LOG* to uart.
    //     esp_log_set_vprintf([](const char* format, va_list args) {
    //         char buf[lm::config::logging::logf_bufsize];
    //         int len = vsnprintf(buf, sizeof(buf), format, args);
    //         if (len > 0) { lograw_uart(buf, len); }
    //         return len; // Return the number of characters written
    //     });

    logging::logbuf.write([](auto& buf){
        buf = fabric::bytebuf(config::logging::ringbuf_size);
    });

    consumers[0].type = logging::consumer::type_t::uart;
    for(auto i = 1; i < consumer_count; ++i)
        consumers[i].type = logging::consumer::type_t::disabled;

    usbd_status_q = fabric::queue<fabric::event>(4);
    usbd_status_q_tok = fabric::bus::subscribe(
        usbd_status_q,
        fabric::topic::usbd,
        {
            usbd::event::cdc_enabled, usbd::event::cdc_disabled,
            usbd::event::hid_enabled, usbd::event::hid_disabled,
        }
    );
}

auto lm::tasks::log::on_ready() -> fabric::managed_task_status
{
    usbd_status_timer.start();
    return fabric::managed_task_status::ok;
}

auto lm::tasks::log::before_sleep() -> fabric::managed_task_status
{
    // 0. Request consumer update if necessary.
    if(usbd_status_timer.is_done()) {
        usbd_status_timer.restart();
        fabric::bus::publish({
            .topic = fabric::topic::usbd,
            .type  = usbd::event::get_status,
        });
    }

    // 1. Update consumers if necessary.
    for(auto& e : usbd_status_q.consume<fabric::event>()) { switch((usbd::event::event_t)e.type){
        case usbd::event::cdc_enabled:
            if(consumers[1].type == logging::consumer::type_t::cdc) break;
            consumers[1].type = logging::consumer::type_t::cdc;
            lm::log::debug("Enabled CDC consumer\n");
            break;
        case usbd::event::hid_enabled:
            if(consumers[2].type == logging::consumer::type_t::hid) break;
            consumers[2].type = logging::consumer::type_t::hid;
            lm::log::debug("Enabled HID consumer\n");
            break;
        case usbd::event::cdc_disabled:
            if(consumers[1].type == logging::consumer::type_t::disabled) break;
            consumers[1].type = logging::consumer::type_t::disabled;
            lm::log::debug("Disabled CDC consumer\n");
            break;
        case usbd::event::hid_disabled:
            if(consumers[2].type == logging::consumer::type_t::disabled) break;
            consumers[2].type = logging::consumer::type_t::disabled;
            lm::log::debug("Disabled HID consumer\n");
            break;
        default: break;
    }}

    // 2. Consume.
    for(auto i = 0; i < consumer_count; ++i)
    {
        auto& consumer = consumers[i];
        auto& logid    = consumer_logids[i];

        while(logid < logs.lifetime_elements)
        {
            auto status = consumer.consume(logs.get_by_id(logid).data);

            if(status == logging::consumer::status::done)
            {
                --logs.get_by_id(logid).refcount;
                ++logid;
            }
            else if(status == logging::consumer::status::blocked) {
                consumer.flush();
                break;
            }
        }
    }

    // 3. Reap.
    while(!logs.empty() && logs.oldest().refcount == 0) {
        logging::logbuf.read([&](auto& logbuf){
            logbuf.free(const_cast<void*>(logs.oldest().data.data)); // This time is okay i swear.
        });
        logs.dequeue();
    }

    return fabric::managed_task_status::ok;
}

auto lm::tasks::log::on_wake() -> fabric::managed_task_status
{
    auto try_get_logs = [&](){
        if(logs.full()) return false;

        buf b;
        logging::logbuf.read([&](auto& logbuf){ b = logbuf.receive(0); });
        if(b.data == nullptr) return false;

        logs.enqueue({ .refcount = consumer_count, .data = b });
        return true;
    };

    while(try_get_logs());

    return fabric::managed_task_status::ok;
}

lm::tasks::log::~log()
{}

auto lm::tasks::logging::consumer::mark_as_done() -> status
{
    offset        = 0;
    total_chunks  = 0;
    failed_chunks = 0;
    return status::done;
}

auto lm::tasks::logging::consumer::consume(buf b) -> status
{
    if(type == type_t::disabled) {
        return status::done;
    }
    else if(type == type_t::uart)
    {
        /// TODO: fixme. How do we chunk for uart?
        if(lm::log::try_dispatch_immediate(board::uart_trace, b, 100000, true)) return mark_as_done();
        else return status::blocked;
    }
    #if CFG_TUD_CDC
    else if(type == type_t::cdc)
    {
        // Skip if CDC is not connected.
        if(!tud_cdc_connected()) { return mark_as_done(); }

        u32 avail = tud_cdc_write_available();
        if(avail == 0) { return status::blocked; }

        u32 chunk = clamp(b.size - offset, 0, avail);
        auto sent = tud_cdc_write((u8*)b.data + offset, chunk);
        offset    += sent;
        ++total_chunks;

        if(sent != chunk){
            ++failed_chunks;
            return status::blocked;
        }
        else if(sent < avail) return mark_as_done();
        /*else*/              return status::blocked;
    }
    #endif
    #if CFG_TUD_HID
    else if(type == type_t::hid)
    {
        if(failed_chunks == 5) return mark_as_done();

        if(!tud_hid_ready()) {
            ++failed_chunks;
            return status::blocked;
        }

        constexpr auto max_chunk_size = 63;
        u8 report[64] = {0};

        int chunk_size = clamp(b.size - offset, 0, max_chunk_size);
        memcpy(report, (u8*)b.data + offset, chunk_size);
        if(tud_hid_report((u8)lm::usbd::hid::hid_reportid::vendor, report, max_chunk_size)) {
            offset += chunk_size;
            failed_chunks = 0; // Reset failure count on success
            if(offset >= b.size) return mark_as_done();
            else                 return status::blocked;
        }

        ++failed_chunks;
        return status::blocked;
    }
    #endif

    // Silence! You know nothing, compiler!
    return status::done;
}

auto lm::tasks::logging::consumer::flush() -> void
{
    /// TODO:
}
