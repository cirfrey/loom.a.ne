#include "lm/fabric/strand.hpp"
#include "lm/fabric/types.hpp"
#include "lm/log.hpp"
#include "lm/core/cvt.hpp"
#include "lm/port.hpp"

#include <thread>
#include <chrono>

// True Windows (MSVC / MinGW) gets high-resolution waitable timer sleep.
// Cygwin has its own POSIX layer — use std::this_thread::sleep_for there.
#if LM_PORT_HOST_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace lm::fabric {

// Wrap std::thread as our opaque handle_t.
struct native_strand {
    std::thread t;
};

// Thread-local: simulates FreeRTOS's "current task" tracking.
thread_local strand::handle_t s_current_strand_handle = nullptr;

auto strand::create(
    create_strand_args const& args,
    void* raw_params
) -> handle_t
{
    // Core affinity and priority are ignored natively — the OS scheduler handles it.
    auto* nt = new native_strand();
    void* final_arg = raw_params
        ? raw_params
        : (managed_strand_params{ .id = args.id, .sleep_ms = args.sleep_ms } | smuggle<void*>);

    nt->t = std::thread([nt, code = args.code, params = final_arg]() {
        s_current_strand_handle = nt;
        code(params);
    });

    if (!nt->t.joinable()) {
        log::error("Spawn strand [%s]...ERROR\n", args.name);
        delete nt;
        return bad_handle;
    }

    return nt;
}

auto strand::get_handle() -> handle_t {
    return s_current_strand_handle;
}

auto strand::reap(handle_t handle) -> reap_status {
    if (!handle) return reap_status::error;

    auto* nt = static_cast<native_strand*>(handle);

    if (nt->t.joinable()) {
        nt->t.join();
        delete nt;
        return reap_status::success;
    }
    return reap_status::error;
}

auto strand::sleep_ms(unsigned long ms) -> void {
    if (ms == 0) {
        std::this_thread::yield();
        return;
    }

#if LM_PORT_HOST_WINDOWS
    // High-resolution waitable timer — avoids the ~15ms minimum granularity
    // of Sleep() on Windows without the spin-wait overhead of busy-looping.
    // Static thread_local: one timer handle per thread, reused every call.
    static thread_local HANDLE timer = CreateWaitableTimerExW(
        NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer) {
        LARGE_INTEGER due;
        due.QuadPart = -10000LL * (LONGLONG)ms; // 100ns units, negative = relative
        SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE);
        WaitForSingleObjectEx(timer, INFINITE, TRUE);
    } else {
        Sleep((DWORD)ms); // fallback if timer creation failed
    }
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

} // namespace lm::fabric

#undef LM_TASK_WINSLEEP
