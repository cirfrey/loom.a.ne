// Use this as a starting point for your task.
#pragma once

#include "lm/fabric.hpp"

#include "lm/core/math.hpp"

#include <array>
#include <initializer_list>
#include <span>

namespace lm::tasks::taskmanager
{
    enum class task_status
    {
        not_created,
        requested_creation,
        created,
        ready,
        running,
        stopped,
        deleted,
    };

    struct task_info
    {
        using task_id_t = decltype(fabric::task_runtime_info::id);
        using task_status_t = task_status;

        fabric::task::task_function_t code;

        fabric::task_constants        constants;
        fabric::task_runtime_info     runtime;

        std::array<task_id_t, 32>     depends = {0}; // TODO: dynamic.

        task_status_t status                = task_status_t::not_created;
        fabric::task::task_handle_t handle  = nullptr;
        bool should_be_running              = false;
    };
}

namespace lm::tasks
{
    struct tman
    {
        using task_info_t = taskmanager::task_info;

        template <st MaxTasks>
        static constexpr auto spawn(
            task_info_t tman_info,
            std::initializer_list<task_info_t> tasks
        ) -> void;

        tman();
        auto do_loop(st max_tasks, std::span<task_info_t>&) -> bool;

    private:
        auto process_events(std::span<task_info_t>&) -> void;
        auto spawn_tasks(std::span<task_info_t>&) -> void;
        auto start_tasks(std::span<task_info_t>&) -> void;

        fabric::queue_t status_q;
        fabric::bus::subscribe_token status_q_tok;
    };
}

template <lm::st MaxTasks>
constexpr auto lm::tasks::tman::spawn(
    task_info_t tman_info,
    std::initializer_list<task_info_t> tasks
) -> void
{
    auto sync = fabric::semaphore::create_binary();

    struct boot_payload {
        task_info_t& tman_info;
        std::initializer_list<task_info_t>& tasks;
        fabric::semaphore& sync;
    } boot{tman_info, tasks, sync};

    fabric::task::create(
        tman_info.constants,
        tman_info.runtime,
        [](void* p){
            auto* boot = static_cast<boot_payload*>(p);

            std::array<task_info_t, MaxTasks> tasks;
            tasks[0] = boot->tman_info;
            st i = 1;
            for(auto& t : boot->tasks) {
                if (i >= MaxTasks) break; // Safety guard
                tasks[i++] = t;
            }
            boot->sync.give(); // Release the parent.

            auto taskview = std::span(tasks.data(), clamp(boot->tasks.size() + 1, 0, MaxTasks));

            tman man;
            while(man.do_loop(MaxTasks, taskview));

            fabric::task::reap(fabric::task::get_handle());
    }, &boot);

    // Block until the child is done copying
    sync.take();
}
