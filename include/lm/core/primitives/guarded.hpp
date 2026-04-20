#pragma once

#include "lm/core/veil.hpp"

#include <shared_mutex>
#include <mutex>

namespace lm
{
    // Very simple wrapper for concurrent resources.
    // Based on CsLibGuarded but just for reeeeally simple types.
    template <typename T>
    struct guarded
    {
        using value_type = T;

        template <typename F, typename... Args> auto write(F&& f, Args...) -> void;
        template <typename F, typename... Args> auto read(F&& f, Args...) const -> void;

    private:
        T data;
        mutable std::shared_mutex mutex;
    };
}

// Impls.

template <typename T>
template <typename F, typename... Args>
auto lm::guarded<T>::write(F&& f, Args... args) -> void
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    f(data, veil::forward<Args>(args)...);
}

template <typename T>
template <typename F, typename... Args>
auto lm::guarded<T>::read(F&& f, Args... args) const -> void
{
    std::shared_lock lock(mutex);
    f(data, veil::forward<Args>(args)...);
}
