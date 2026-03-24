#pragma once

#include <shared_mutex>
#include <mutex>

namespace espy::utils
{
    // Very simple wrapper for concurrent resources.
    template <typename T>
    struct guarded
    {
        template <typename F> auto write(F&& f) -> void;
        template <typename F> auto read(F&& f) const -> void;

    private:
        T data;
        mutable std::shared_mutex mutex;
    };
}

// Impls.

template <typename T>
template <typename F>
auto espy::utils::guarded<T>::write(F&& f) -> void
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    f(data);
}

template <typename T>
template <typename F>
auto espy::utils::guarded<T>::read(F&& f) const -> void
{
    std::shared_lock lock(mutex);
    f(data);
}
