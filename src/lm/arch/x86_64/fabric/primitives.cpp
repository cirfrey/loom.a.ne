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

// --- Bytebuf Mock ---
// Simulates FreeRTOS RINGBUF_TYPE_BYTEBUF.
// FreeRTOS gives out internal pointers on receive() and requires a free() call.
// We simulate this by heap-allocating discrete chunks.
struct native_bytebuf {
    std::mutex mtx;
    std::condition_variable cv_send;
    std::condition_variable cv_recv;

    u8* storage;         // Pre-allocated contiguous block
    st capacity;

    st head = 0;         // Where we write next (Send)
    st tail = 0;         // Where we read next (Receive)
    st wrap_count = 0;   // Where the "released" memory ends
    st bytes_available = 0; // Bytes ready to be read
    st bytes_in_flight = 0; // Bytes received but not yet freed

    // We track metadata for pointers currently held by the user
    struct segment { void* ptr; st size; };
    std::vector<segment> active_segments;
};

bytebuf::bytebuf(st bytes) {
    auto* nb = new native_bytebuf();
    nb->capacity = bytes;
    nb->storage = new u8[bytes];
    nb->bytes_available = 0;
    nb->bytes_in_flight = 0;
    impl = nb;
}

auto bytebuf::capacity() const -> st
{
    if(impl) return ((native_bytebuf*)impl)->capacity;
    return 0;
}

auto bytebuf::occupancy() const -> st
{
    if(impl) return ((native_bytebuf*)impl)->bytes_available + ((native_bytebuf*)impl)->bytes_in_flight;
    return 0;
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
        delete[] nb->storage;
        delete nb;
    }
}

auto bytebuf::send(buf data, u32 timeout) const -> bool {
    auto* nb = static_cast<native_bytebuf*>(impl);
    std::unique_lock lock(nb->mtx);

    // Wait until there is enough contiguous OR wrapped space
    // FreeRTOS bytebufs require the space to be contiguous for the send
    bool acquired = nb->cv_send.wait_for(lock, ms_to_chrono(timeout), [&]{
        st total_free = nb->capacity - (nb->bytes_available + nb->bytes_in_flight);
        return total_free >= data.size;
    });

    if (!acquired) return false;

    // Handle Circular Logic: If we can't fit at the end, we wrap to the front
    if (nb->head + data.size > nb->capacity) {
        // Simple mock: we'll force wrap-around if it doesn't fit linearly
        // Note: Real FreeRTOS might split this or wait for linear space.
        // For efficiency, we simulate the linear copy:
        st space_at_end = nb->capacity - nb->head;
        if(space_at_end > 0) {
            std::memcpy(nb->storage + nb->head, data.data, space_at_end);
            std::memcpy(nb->storage, (u8*)data.data + space_at_end, data.size - space_at_end);
        }
    } else {
        std::memcpy(nb->storage + nb->head, data.data, data.size);
    }

    nb->head = (nb->head + data.size) % nb->capacity;
    nb->bytes_available += data.size;

    nb->cv_recv.notify_one();
    return true;
}

auto bytebuf::receive(u32 timeout) const -> buf {
    auto* nb = static_cast<native_bytebuf*>(impl);
    std::unique_lock lock(nb->mtx);

    bool acquired = nb->cv_recv.wait_for(lock, ms_to_chrono(timeout), [&]{
        return nb->bytes_available > 0;
    });

    if (!acquired) return { nullptr, 0 };

    // To simulate RINGBUF_TYPE_BYTEBUF, we return the largest contiguous block
    st chunk_size = std::min(nb->bytes_available, nb->capacity - nb->tail);
    void* ptr = nb->storage + nb->tail;

    // Move from 'available' to 'in-flight'
    nb->bytes_available -= chunk_size;
    nb->bytes_in_flight += chunk_size;
    nb->active_segments.push_back({ptr, chunk_size});

    nb->tail = (nb->tail + chunk_size) % nb->capacity;

    return { .data = ptr, .size = chunk_size };
}

auto bytebuf::free(void* data) const -> void {
    if (!data) return;
    auto* nb = static_cast<native_bytebuf*>(impl);
    std::unique_lock lock(nb->mtx);

    auto it = std::find_if(nb->active_segments.begin(), nb->active_segments.end(),
                           [data](const native_bytebuf::segment& s) { return s.ptr == data; });

    if (it != nb->active_segments.end()) {
        nb->bytes_in_flight -= it->size;
        nb->active_segments.erase(it);

        // Signal that space has been reclaimed
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

auto queue_t::slots() const -> st
{
    auto* nq = static_cast<native_queue*>(impl);
    return capacity() - nq->items.size(); // TODO: multithread me.
}

auto queue_t::capacity() const -> st { return max_elements; }
auto queue_t::element_size() const -> st { return element_size_in_bytes; }

// --- Semaphore Mock ---
struct native_sem {
    std::mutex mtx = std::mutex{};
    std::condition_variable cv = std::condition_variable{};
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
