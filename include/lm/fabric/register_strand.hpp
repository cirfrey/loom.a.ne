
// Wrapper around the bus registration protocol so you can do it easy and fun and nice.
#pragma once

#include "lm/fabric/strand.hpp"
#include "lm/config.hpp"
#include "lm/core/hash.hpp"
#include <initializer_list>

namespace lm::fabric
{
    struct depends
    {
        using depends_t = fabric::topic::framework_t::request_register_strand::depends_t;
        using status    = fabric::topic::framework_t::strand_status::status_t;

        depends_t underlying_depends;

        constexpr operator depends_t() { return underlying_depends; }

        constexpr depends(u8 other_id, status other_status)
        {
            underlying_depends.other = other_id;
            underlying_depends.other_status = other_status;
            underlying_depends.my_status = status::running;
        }
        constexpr depends(text other_name, status other_status)
        {
            underlying_depends.other = fnv1a_32(other_name);
            underlying_depends.other_status = other_status;
            underlying_depends.my_status = status::running;
        }
        constexpr auto to_be(status s) -> depends&
        {
            underlying_depends.my_status = s;
            return *this;
        }
    };


    // ── Result ────────────────────────────────────────────────────────────────

    struct register_result
    {
        using result_t = fabric::topic::framework_t::response_register_strand::result_t;

        result_t result = result_t::request_malformed;
        u8       id     = 0;

        explicit operator bool() const {
            return result == result_t::ok;
        }
        auto ok()          const -> bool { return result == result_t::ok; }
        auto assigned_id() const -> u8   { return id; }
    };

    // ── Params ────────────────────────────────────────────────────────────────

    struct register_params
    {
        text                                      name          = {};
        u16                                       stack_size    = 128;   // words
        u16                                       sleep_ms      = 10;
        u8                                        priority      = 5;
        u16                                       core_affinity = 0;
        bool                                      start         = true;
        fabric::strand::function_t                code          = nullptr;
        std::initializer_list<depends::depends_t> depends       = {};


        // Advanced — leave as defaults unless you have a reason
        static constexpr auto autodiscover_manager = -st{0};
        static constexpr auto derive_timeout       = 0;
        st  manager_id     = autodiscover_manager;
        u32 timeout_micros = derive_timeout;
        u8  caller_id      = 255;   // your strand's id, used for response filtering
        st  max_attempts   = 3;
    };

    // ── Main API ─────────────────────────────────────────────────────────────


    template <typename Strand>
    [[nodiscard]] auto register_strand(register_params const&) -> register_result;
    [[nodiscard]] auto register_strand(register_params const&) -> register_result;

    // Convenience: invalidate the cached manager, force fresh discovery.
    // Call this if you get manager_cant_handle_more_strands and want to re-probe.
    auto invalidate_manager_cache() -> void;
}


template <typename Strand>
[[nodiscard]] auto lm::fabric::register_strand(register_params const& p) -> register_result
{
    auto p2 = p;
    p2.code = strand::managed<Strand>();
    return register_strand(p2);
}
