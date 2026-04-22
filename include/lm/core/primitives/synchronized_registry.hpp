#pragma once

#include "lm/core/types.hpp"

#include "lm/core/primitives/bitset.hpp"
#include "lm/fabric/primitives.hpp"

#include <initializer_list>

namespace lm
{
    struct synchronized_registry_reserve_result
    {
        enum status_t : u8 { ok, error } status;
        st id;

        constexpr auto is_ok()     { return status == ok; }
        constexpr auto has_error() { return status == error; }
    };

    template <st Entries>
    struct synchronized_registry
    {
        using result = synchronized_registry_reserve_result;

        static constexpr auto entries = Entries;

        synchronized_registry(std::initializer_list<st> initially_reserved);
        // Init needs to be deferred since we don't really know if the
        // loom is ready to use primitives yet (running before scheduler or something
        // crazy like that).
        auto init() -> void;

        [[nodiscard]] auto reserve(st id) -> result;
        [[nodiscard]] auto reserve()      -> result;

        auto unreserve(st id) -> void;

        bitset<entries>   bs;
        fabric::semaphore bs_mtx;
    };
}


template <lm::st Entries>
lm::synchronized_registry<Entries>::synchronized_registry(std::initializer_list<st> initially_reserved)
{
    for(auto e : initially_reserved) bs.set(e);
}

template <lm::st Entries>
auto lm::synchronized_registry<Entries>::init() -> void
{
    bs_mtx = fabric::semaphore::create_binary();
    bs_mtx.give();
}

template <lm::st Entries>
auto lm::synchronized_registry<Entries>::reserve(st id) -> result
{
    bs_mtx.take();
    bool taken = bs.test(id);
    if(!taken) bs.set(id);
    bs_mtx.give();
    return { .status = taken ? result::error : result::ok, .id = id };
}

template <lm::st Entries>
auto lm::synchronized_registry<Entries>::reserve() -> result
{
    bs_mtx.take();
    auto pos = bs.find_first_clear(0);
    if(pos != decltype(bs)::npos && pos < entries) bs.set(pos);
    bs_mtx.give();

    if(pos != decltype(bs)::npos && pos < entries) {
        return { .status = result::ok, .id = pos };
    } else {
        return { .status = result::error, .id = 0 };
    }
}

template <lm::st Entries>
auto lm::synchronized_registry<Entries>::unreserve(st id) -> void
{
    bs_mtx.take();
    bs.clear(id);
    bs_mtx.give();
}
