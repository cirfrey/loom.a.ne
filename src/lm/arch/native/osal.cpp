#include "lm/fabric/primitives.hpp"

#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <chrono>
#include <cstring>

namespace lm::fabric {

// Helper to convert timeout to chrono
static auto ms_to_chrono(u32 timeout) {
    // If timeout is "portMAX_DELAY" (usually 0xFFFFFFFF), wait indefinitely (or a very long time)
    if (timeout == 0xFFFFFFFF) return std::chrono::milliseconds(1000 * 60 * 60 * 24);
    return std::chrono::milliseconds(timeout);
}

// Simulates FreeRTOS RINGBUF_TYPE_BYTEBUF.
struct chunk_info {
    void* ptr;
    st size;
    bool freed;
};

struct native_bytebuf {
    std::mutex mtx;
    std::condition_variable cv_send;
    std::condition_variable cv_recv;

    u8* buffer = nullptr; // Single continuous allocation
    st capacity = 0;

    st write_pos = 0;     // Write head
    st read_pos = 0;      // Read head
    st free_pos = 0;      // Tail of used memory

    st used_bytes = 0;    // Total bytes occupied (between free_pos and write_pos)
    st unread_bytes = 0;  // Bytes written but not yet received (between read_pos and write_pos)

    // Tracks chunks given to the consumer to handle out-of-order frees securely
    std::deque<chunk_info> active_chunks;
};

bytebuf::bytebuf(st bytes) {
    auto* nb = new native_bytebuf();
    nb->capacity = bytes;
    nb->buffer = new u8[bytes]; // Allocate ring buffer memory ONCE
    impl = nb;
}

bytebuf::bytebuf(bytebuf&& o) { *this = static_cast<bytebuf&&>(o); }

auto bytebuf::operator=(bytebuf&& o) -> bytebuf& {
    auto timpl = impl;
    impl = o.impl;
    o.impl = timpl;
    return *this;
}

bytebuf::~bytebuf() {
    if (impl) {
        auto* nb = static_cast<native_bytebuf*>(impl);
        delete[] nb->buffer; // Clean up the single buffer block
        delete nb;
    }
}

auto bytebuf::send(buf data, u32 timeout) const -> bool {
    auto* nb = static_cast<native_bytebuf*>(impl);
    std::unique_lock lock(nb->mtx);

    bool acquired = nb->cv_send.wait_for(lock, ms_to_chrono(timeout), [&]{
        return (nb->capacity - nb->used_bytes) >= data.size;
    });

    if (!acquired) return false;

    // Handle wrap-around memory copy into the circular buffer
    st first_part = std::min(data.size, nb->capacity - nb->write_pos);
    std::memcpy(nb->buffer + nb->write_pos, data.data, first_part);

    if (first_part < data.size) {
        st second_part = data.size - first_part;
        std::memcpy(nb->buffer, static_cast<u8*>(data.data) + first_part, second_part);
    }

    nb->write_pos = (nb->write_pos + data.size) % nb->capacity;
    nb->used_bytes += data.size;
    nb->unread_bytes += data.size;

    nb->cv_recv.notify_one();
    return true;
}

auto bytebuf::receive(u32 timeout) const -> buf {
    auto* nb = static_cast<native_bytebuf*>(impl);
    std::unique_lock lock(nb->mtx);

    bool acquired = nb->cv_recv.wait_for(lock, ms_to_chrono(timeout), [&]{
        return nb->unread_bytes > 0;
    });

    if (!acquired) return { nullptr, 0 };

    // Return a contiguous chunk up to the end of the physical buffer boundary
    st chunk_size = std::min(nb->unread_bytes, nb->capacity - nb->read_pos);
    u8* chunk_ptr = nb->buffer + nb->read_pos;

    // Record the chunk so we can safely advance the tail pointer later
    nb->active_chunks.push_back({chunk_ptr, chunk_size, false});

    nb->read_pos = (nb->read_pos + chunk_size) % nb->capacity;
    nb->unread_bytes -= chunk_size;

    return { .data = chunk_ptr, .size = chunk_size };
}

auto bytebuf::free(void* data) const -> void {
    if (!data || !impl) return;
    auto* nb = static_cast<native_bytebuf*>(impl);
    std::unique_lock lock(nb->mtx);

    // 1. Find the chunk and mark it as ready to be freed
    for (auto& chunk : nb->active_chunks) {
        if (chunk.ptr == data) {
            chunk.freed = true;
            break;
        }
    }

    // 2. Advance the actual `free_pos` only for contiguous freed chunks at the front.
    // This prevents the write head from overwriting memory that was received after this chunk,
    // but hasn't been freed yet (handling out-of-order frees properly).
    bool space_freed = false;
    while (!nb->active_chunks.empty() && nb->active_chunks.front().freed) {
        auto& chunk = nb->active_chunks.front();
        nb->free_pos = (nb->free_pos + chunk.size) % nb->capacity;
        nb->used_bytes -= chunk.size;
        nb->active_chunks.pop_front();
        space_freed = true;
    }

    // Only wake up senders if we actually cleared up space at the tail
    if (space_freed) {
        nb->cv_send.notify_all();
    }
}

auto bytebuf::initialized() const -> bool { return impl != nullptr; }

// --- Queue Mock ---
struct native_queue {
    std::mutex mtx;
    std::condition_variable cv_send;
    std::condition_variable cv_recv;
    std::deque<std::vector<u8>> items;
    st max_elements;
    st element_size;
};

queue_t::queue_t(st max_elements, st element_size_in_bytes)
    : element_size_in_bytes{ element_size_in_bytes }
    , max_elements{ max_elements }
{
    auto* nq = new native_queue();
    nq->max_elements = max_elements;
    nq->element_size = element_size_in_bytes;
    impl = nq;
}

queue_t::queue_t(queue_t&& o) { *this = static_cast<queue_t&&>(o); }

auto queue_t::operator=(queue_t&& o) -> queue_t& {
    auto timpl = impl; impl = o.impl; o.impl = timpl;
    auto tel = element_size_in_bytes; element_size_in_bytes = o.element_size_in_bytes; o.element_size_in_bytes = tel;
    auto tmel = max_elements; max_elements = o.max_elements; o.max_elements = tmel;
    return *this;
}

queue_t::~queue_t() {
    if (impl) delete static_cast<native_queue*>(impl);
}

auto queue_t::send(void const* item, u32 timeout) -> bool {
    auto* nq = static_cast<native_queue*>(impl);
    std::unique_lock lock(nq->mtx);

    bool acquired = nq->cv_send.wait_for(lock, ms_to_chrono(timeout), [&]{
        return nq->items.size() < nq->max_elements;
    });

    if (!acquired) return false;

    auto* data_ptr = static_cast<const u8*>(item);
    nq->items.emplace_back(data_ptr, data_ptr + nq->element_size);

    nq->cv_recv.notify_one();
    return true;
}

auto queue_t::receive(void* into, u32 timeout) -> bool {
    auto* nq = static_cast<native_queue*>(impl);
    std::unique_lock lock(nq->mtx);

    bool acquired = nq->cv_recv.wait_for(lock, ms_to_chrono(timeout), [&]{
        return !nq->items.empty();
    });

    if (!acquired) return false;

    std::memcpy(into, nq->items.front().data(), nq->element_size);
    nq->items.pop_front();

    nq->cv_send.notify_one();
    return true;
}

auto queue_t::capacity() const -> st { return max_elements; }
auto queue_t::element_size() const -> st { return element_size_in_bytes; }

// --- Semaphore Mock ---
struct native_sem {
    std::mutex mtx;
    std::condition_variable cv;
    u32 count;
    u32 max_count;
};

auto semaphore::create_binary() -> semaphore {
    semaphore s;
    auto* ns = new native_sem{ .count = 0, .max_count = 1 };
    s.impl = ns;
    return s;
}

auto semaphore::create_counting(u32 max, u32 initial) -> semaphore {
    semaphore s;
    auto* ns = new native_sem{ .count = initial, .max_count = max };
    s.impl = ns;
    return s;
}

semaphore::semaphore(semaphore&& o) noexcept : impl(o.impl) { o.impl = nullptr; }

auto semaphore::operator=(semaphore&& o) noexcept -> semaphore& {
    auto timpl = impl; impl = o.impl; o.impl = timpl;
    return *this;
}

semaphore::~semaphore() {
    if (impl) delete static_cast<native_sem*>(impl);
}

auto semaphore::take(u32 timeout_ms) const -> bool {
    if (!impl) return false;
    auto* ns = static_cast<native_sem*>(impl);
    std::unique_lock lock(ns->mtx);

    return ns->cv.wait_for(lock, ms_to_chrono(timeout_ms), [&]{
        if (ns->count > 0) {
            ns->count--;
            return true;
        }
        return false;
    });
}

auto semaphore::give() const -> void {
    if (!impl) return;
    auto* ns = static_cast<native_sem*>(impl);
    std::unique_lock lock(ns->mtx);

    if (ns->count < ns->max_count) {
        ns->count++;
        ns->cv.notify_one();
    }
}

auto semaphore::initialized() const -> bool { return impl != nullptr; }

} // namespace lm::fabric

#include "lm/fabric/task.hpp"
#include "lm/fabric/types.hpp"
#include "lm/fabric/bus.hpp"
#include "lm/log.hpp"
#include "lm/core/cvt.hpp"

#include <thread>
#include <chrono>

namespace lm::fabric {

// Wrap the std::thread to serve as our opaque task_handle_t
struct native_task {
    std::thread t;
};

// Thread-local storage to simulate FreeRTOS tracking the "Current Task"
thread_local task::task_handle_t s_current_task_handle = nullptr;

auto task::create(
    task_constants const& cfg,
    task_runtime_info const& info,
    task_function_t task_func,
    void* taskarg
) -> task_handle_t
{
    // Note: Core affinity and priority are ignored natively as the OS scheduler handles it.

    auto* nt = new native_task();
    void* final_arg = taskarg ? taskarg : (info | smuggle<void*>);

    // Spin up the native thread
    nt->t = std::thread([nt, task_func, final_arg]() {
        // First action: register this thread's handle
        s_current_task_handle = nt;

        // Execute the user task
        task_func(final_arg);
    });

    if (!nt->t.joinable()) {
        log::error("Spawn task [%s]...ERROR natively\n", cfg.name);
        delete nt;
        return nullptr;
    }

    bus::publish(fabric::event{
        .topic = topic::task,
        .type = event::created,
        .sender_id = info.id,
    }.with_payload<status_event>({ .handle = nt }));

    return nt;
}

auto task::get_handle() -> task_handle_t {
    return s_current_task_handle;
}

auto task::reap(task_handle_t handle) -> void {
    if (!handle) return;
    auto* nt = static_cast<native_task*>(handle);

    // Natively, you cannot forcefully kill a thread safely without risking deadlocks
    // (unlike vTaskDelete). We detach it so the OS can clean it up when it exits.
    // The user task MUST be cooperating and listening for shutdown flags.
    if (nt->t.joinable()) {
        nt->t.detach();
    }

    delete nt;
}


#include <chrono>
#include <thread>

#if defined(_WIN32) && !defined(__CYGWIN__)
    // Native Windows (MSVC, MinGW, Clang-cl)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    // POSIX (Cygwin, Linux, macOS, Android)
    #include <time.h>
    #include <errno.h>
#endif

auto task::sleep_ms(unsigned long ms) -> void {
    if (ms == 0) {
        std::this_thread::yield();
        return;
    }

#if defined(_WIN32) && !defined(__CYGWIN__)
    // --- NATIVE WINDOWS PATH ---
    // Using a static handle to avoid re-creating the timer every call
    static HANDLE timer = CreateWaitableTimerExW(NULL, NULL,
                          CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                          TIMER_ALL_ACCESS);
    if (timer) {
        LARGE_INTEGER due;
        due.QuadPart = -10000LL * ms;
        SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
        WaitForSingleObject(timer, INFINITE);
    } else {
        Sleep(ms);
    }
#else
    // --- POSIX / CYGWIN PATH ---
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    // nanosleep can be interrupted by signals; we loop until done
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR);
#endif
}

} // namespace lm::fabric
