#include "lm/tasks/healthmon.hpp"

#include "lm/config.hpp"
#include "lm/task.hpp"
#include "lm/tasks/logging.hpp"
#include "lm/chip/memory.hpp"
#include "lm/board.hpp"
#include "lm/core/cvt.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <vector>
#include <algorithm>

// Helper: Map FreeRTOS state codes to readable chars
constexpr char get_state_char(eTaskState state) {
    switch (state) {
        case eRunning:   return 'X'; // Executing right now
        case eReady:     return 'R'; // Ready to run
        case eBlocked:   return 'B'; // Waiting for event/time
        case eSuspended: return 'S'; // Halted
        case eDeleted:   return 'D';
        default:         return '?';
    }
}

auto lm::healthmon::init() -> void { lm::task::create(lm::config::task::healthmon, lm::healthmon::task); }
auto lm::healthmon::task(lm::task::config const& cfg) -> void
{
    using tc = lm::task::event::task_command;
    auto tc_bus = tc::make_bus();
    tc::wait_for_start(tc_bus, cfg.id);

    while(!tc::should_stop(tc_bus, cfg.id))
    {
        // Ideally this should sleep very little, accumulating
        // info from event bus and keeping track of it,
        // then - every once in a while - make a report on it
        // with some heavier processing.
        lm::task::sleep_ms(cfg.sleep_ms);

        // --- PART A: Global Heap Health ---
        auto total     = chip::memory::total();
        auto free_heap = chip::memory::free();

        lm::logging::logf("RAM: %zu/%zukb [%.1f%%] | Peak: %zukb | MaxBlock: %zukb\n",
            (total - free_heap)/1024, total/1024, (total-free_heap)/(total | to<f32>)*100,
            chip::memory::peak_used()/1024, chip::memory::largest_free_block()/1024);

        if (free_heap < 15000) {
            lm::logging::logf("[WARN] LOW MEMORY! Instability possible.\n");
        }

        continue;

        /// TODO: add eventbus stats (memory, listeners, events published by topic, etc).
        /// TODO: add task cpu usage %.

        /// TODO: we can replace this whole forensics part with a few wrappers and keeping an eye on lm::bus::task (will have to maintain our own internal state OR query sysman).
        // --- PART B: Task Stack Forensics ---
        // Snapshot the scheduler
        UBaseType_t task_count = uxTaskGetNumberOfTasks();

        // Allocate buffer dynamically to save static RAM
        // Note: We use a vector, so this task needs ~4KB stack itself to handle the vector overhead + printf
        std::vector<TaskStatus_t> status_array(task_count);
        uint32_t total_runtime = 0;

        task_count = uxTaskGetSystemState(
            status_array.data(),
            task_count,
            &total_runtime
        );

        // Sort by "Least Stack Remaining" (Ascending) to highlight danger zones
        std::sort(status_array.begin(), status_array.end(),
            [](const TaskStatus_t& a, const TaskStatus_t& b) {
                return a.usStackHighWaterMark < b.usStackHighWaterMark;
            });

        lm::logging::logf("%-16s | %-3s | %-3s | %s\n", "Task Name", "St", "Pri", "Stack (Bytes Left)");
        lm::logging::logf("--------------------------------------------------\n");

        bool alert_triggered = false;

        for (UBaseType_t i = 0; i < task_count; i++) {
            auto& t = status_array[i];

            // On ESP-IDF, usStackHighWaterMark is in BYTES.
            uint32_t stack_left = t.usStackHighWaterMark;

            lm::logging::logf("%-16s |  %c  | %3u | %u\n",
                t.pcTaskName,
                get_state_char(t.eCurrentState),
                t.uxCurrentPriority,
                stack_left
            );

            // Threshold check (configurable in config.hpp ideally)
            if (stack_left < 256) {
                alert_triggered = true;
            }
        }

        if (alert_triggered) {
            lm::logging::logf("\n[!!!] CRITICAL: One or more tasks are near stack overflow (<256B)!\n");
        }

        lm::logging::logf("==============================\n");
    }
}
