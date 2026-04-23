#include "lm/fabric/register_strand.hpp"

#include "lm/fabric.hpp"
#include "lm/chip/time.hpp"
#include "lm/config.hpp"
#include "lm/log.hpp"

#include <cstring>

namespace
{
    using namespace lm;

    // ── Manager cache ─────────────────────────────────────────────────────────
    // One cache entry is enough for the common case.
    // Invalidated on manager_cant_handle_more_strands.

    struct manager_cache_t {
        bool valid           = false;
        u8   manager_id      = 0;
        u16  safe_timeout_ms = 10;   // conservative default
        st   available_slots = 0;
    };

    manager_cache_t g_manager_cache{};

    // ── Seqnum ───────────────────────────────────────────────────────────────

    auto next_seqnum() -> u8 {
        static u8 n = 0;
        return ++n;   // wraps at 255 — fine, it's a correlation key not an ID
    }

    // ── Manager discovery ────────────────────────────────────────────────────

    auto discover_manager() -> bool
    {
        using request  = fabric::topic::framework_t::request_manager_announce;
        using response = fabric::topic::framework_t::response_manager_announce;

        auto seq      = next_seqnum();
        auto resp_q   = fabric::queue<fabric::event>(4);
        auto resp_tok = fabric::bus::subscribe(
            resp_q,
            fabric::topic::framework,
            {response::type},
            [](void* ud, std::span<fabric::event> events) {
                auto seq = ud | unsmuggle<u8>;

                auto data = events[0].get_payload<response>();
                if(data.seqnum == seq)
                    return fabric::bus::pass;
                return fabric::bus::filter;
            },
            seq | smuggle<void*>
        );


        fabric::bus::publish(fabric::event{
            .topic = fabric::topic::framework,
            .type  = request::type,
        }.with_payload(request{ .seqnum = seq }));


        // Collect all responses within the window, pick the richest manager.
        fabric::strand::sleep_ms(config.framework.manager_announce_window_ms);
        manager_cache_t best{};

        for(auto const& e : resp_q.consume<fabric::event>()) {
            auto& data = e.get_payload<response>();
            if(!best.valid || data.available_slots > best.available_slots)
                best = {true, e.strand_id, data.safe_timeout_ms, data.available_slots};
        }

        if(!best.valid) return false;
        g_manager_cache = best;
        return true;
    }

    // ── Event chain builder ───────────────────────────────────────────────────

    using request  = lm::fabric::topic::framework_t::request_register_strand;
    using response = lm::fabric::topic::framework_t::response_register_strand;

    // Maximum chain length: header + extdata + extname + enough extdepends for max_depends
    static constexpr st max_chain_len =
        3 + (config_t::strandman_t::max_depends + request::depends_per_ext - 1)
              / request::depends_per_ext;

    struct event_chain {
        fabric::event events[max_chain_len] = {};
        st            len                   = 0;

        auto span() -> std::span<fabric::event> { return {events, len}; }
    };

    auto build_chain(
        lm::fabric::register_params const& p,
        u8 manager_id,
        u8 seq,
        u32 timeout_us
    ) -> event_chain
    {
        event_chain c;

        // ── Header ────────────────────────────────────────────────────────────
        c.events[c.len++] = fabric::event{
            .topic     = fabric::topic::framework,
            .type      = request::type,
            .strand_id = p.caller_id,
        }.with_payload(request{
            .seqnum        = seq,
            .manager_id    = manager_id,
            .autoassign_id = true,
            .timeout_micros = timeout_us,
        });

        // ── extdata ───────────────────────────────────────────────────────────
        c.events[c.len++] = fabric::event::extension(request::extdata{
            .stack_size      = p.stack_size,
            .sleep_ms        = p.sleep_ms,
            .request_running = p.start,
            .strand          = p.code,
            .priority        = p.priority,
            .core_affinity   = p.core_affinity,
        });

        // ── extname ───────────────────────────────────────────────────────────
        request::extname extname{};
        // TODO: wtf is this doing here?
        __builtin_memcpy(extname.name, p.name.data,
                        p.name.size < sizeof(extname.name) - 1
                        ? p.name.size : sizeof(extname.name) - 1);
        c.events[c.len++] = fabric::event::extension(extname);

        // ── extdepends ────────────────────────────────────────────────────────
        // All depends go here — one extdepends event per depends_per_ext depends.
        st dep_idx = 0;
        for(auto const& d : p.depends) {
            st slot = dep_idx % request::depends_per_ext;
            if(slot == 0) {
                if(c.len >= max_chain_len) break;
                c.events[c.len++] = fabric::event::extension(request::extdepends{});
            }
            auto& xd = c.events[c.len - 1].as_extension<request::extdepends>();
            xd.depends[slot] = d;
            ++dep_idx;
        }

        return c;
    }
}

// ── Public ───────────────────────────────────────────────────────────────────

auto lm::fabric::invalidate_manager_cache() -> void
{
    g_manager_cache = {};
}

auto lm::fabric::register_strand(register_params const& p) -> register_result
{
    // ── Resolve manager ───────────────────────────────────────────────────────
    u8  manager_id = p.manager_id;
    u16 mgr_sleep  = 10;

    if(manager_id != register_params::autodiscover_manager) {
        // Explicit — trust the caller, use conservative sleep estimate
        mgr_sleep = 10;
    } else {
        if(!g_manager_cache.valid) {
            if(!discover_manager()) {
                log::warn("[strand::register_with] no manager found\n");
                return {response::request_malformed};
            }
        }
        manager_id = g_manager_cache.manager_id;
        mgr_sleep  = g_manager_cache.safe_timeout_ms;
    }

    // ── Timeout: never shorter than 2 full manager cycles ────────────────────
    auto const timeout_us = p.timeout_micros == register_params::derive_timeout
        ? (u32)(mgr_sleep * 2 * 1000)
        : p.timeout_micros;

    // ── Subscribe for response before publishing ──────────────────────────────
    struct smuggled_t {
        u8 seq;
        u8 manager_id;
    } smuggled = {
        next_seqnum(),
        manager_id
    };
    auto resp_q   = fabric::queue<fabric::event>(1);
    auto resp_tok = fabric::bus::subscribe(
        resp_q,
        fabric::topic::framework,
        {response::type},
        [](void* ud, std::span<fabric::event> events){
            auto smuggled = ud | unsmuggle<smuggled_t>;
            auto data = events[0].get_payload<response>();
            if(data.seqnum == smuggled.seq && events[0].strand_id == smuggled.manager_id)
                return fabric::bus::pass;
            return fabric::bus::filter;
        },
        smuggled | smuggle<void*>
    );

    auto chain = build_chain(p, manager_id, smuggled.seq, timeout_us);

    // ── Publish + poll with exponential backoff ───────────────────────────────
    for(st attempt = 0; attempt < p.max_attempts; ++attempt)
    {
        fabric::bus::publish(chain.span());

        auto deadline = chip::time::uptime() + timeout_us;
        while(chip::time::uptime() < deadline)
        {
            for(auto const& e : resp_q.consume<fabric::event>()) {
                auto& rep = e.get_payload<response>();

                if(rep.result == response::ok) {
                    if(g_manager_cache.available_slots > 0)
                        --g_manager_cache.available_slots;
                    return {response::ok, rep.id};
                }

                if(rep.result == response::manager_cant_handle_more_strands) {
                    invalidate_manager_cache();
                    if(p.manager_id == p.autodiscover_manager) {
                        auto newp = p;
                        newp.max_attempts -= attempt + 1;
                        return register_strand(newp);
                    }
                }

                return {rep.result};   // any other error is definitive, don't retry
            }
            fabric::strand::sleep_ms(1);
        }


        log::warn("Attempt %d/%d timed out\n", (int)(attempt + 1), (int)p.max_attempts);

        // Timed out — backoff
        // TODO: dynamic backoff.
        fabric::strand::sleep_ms(10);
    }

    return {response::request_malformed};
}
