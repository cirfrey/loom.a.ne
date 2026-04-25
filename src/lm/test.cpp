#include "lm/test.hpp"
#include "lm/log.hpp"
#include "lm/core/hash.hpp"

#include "lm/config.hpp"

// ── Registry storage ──────────────────────────────────────────────────────────
// Static arrays, zero-initialized before dynamic init.
// max_suites is a hard limit — no heap allocation.

namespace
{
    static constexpr lm::st max_suites = lm::config.test.max_unit_suites;

    struct unit_entry {
        const char*          name = nullptr;
        lm::u32              hash = 0;
        lm::test::unit_suite_fn fn = nullptr;
    };

    struct mfg_entry {
        const char*         name = nullptr;
        lm::u32             hash = 0;
        lm::test::mfg_suite_fn fn = nullptr;
    };

    unit_entry unit_registry[max_suites] = {};
    mfg_entry  mfg_registry[max_suites]  = {};
    lm::st     unit_count = 0;
    lm::st     mfg_count  = 0;

    auto log_suite_result(lm::test::suite_result const& r) -> void
    {
        if(r.failed == 0) {
            lm::log::test("[PASS] %s (%lu checks)\n",
                r.name,
                (unsigned long)r.passed);
            return;
        }

        lm::log::test("[FAIL] %s — %lu/%lu failed\n",
            r.name,
            (unsigned long)r.failed,
            (unsigned long)(r.passed + r.failed));

        for(lm::st i = 0; i < r.failure_count; ++i) {
            auto& f = r.failures[i];
            if(f.context) {
                lm::log::test("  [%s:%lu] %s | %s\n",
                    f.file, (unsigned long)f.line, f.expr, f.context);
            } else {
                lm::log::test("  [%s:%lu] %s\n",
                    f.file, (unsigned long)f.line, f.expr);
            }
        }

        if(r.failed > r.failure_count) {
            lm::log::test("  ... and %lu more (increase suite_result::max_failures to see all)\n",
                (unsigned long)(r.failed - r.failure_count));
        }
    }
}

// ── Registration ──────────────────────────────────────────────────────────────

auto lm::test::register_unit_suite(const char* name, unit_suite_fn fn) -> void
{
    if(unit_count >= max_suites) return;
    unit_registry[unit_count++] = {
        name,
        lm::fnv1a_32({name, __builtin_strlen(name)}),
        fn,
    };
}

auto lm::test::register_mfg_suite(const char* name, mfg_suite_fn fn) -> void
{
    if(mfg_count >= max_suites) return;
    mfg_registry[mfg_count++] = {
        name,
        lm::fnv1a_32({name, __builtin_strlen(name)}),
        fn,
    };
}

// ── Unit runner ───────────────────────────────────────────────────────────────

auto lm::test::run_unit() -> run_result
{
    run_result total{};

    lm::log::test("─── lm::test — %lu suites ───\n",
        (unsigned long)unit_count);

    for(lm::st i = 0; i < unit_count; ++i) {
        auto r = unit_registry[i].fn();
        log_suite_result(r);
        ++total.suites_run;
        total.checks_passed += r.passed;
        total.checks_failed += r.failed;
    }

    if(total.checks_failed == 0) {
        lm::log::test("─── ALL PASS  %lu checks  %lu suites ───\n",
            (unsigned long)total.checks_passed,
            (unsigned long)total.suites_run);
    } else {
        lm::log::test("─── FAILED  %lu/%lu checks  %lu suites ───\n",
            (unsigned long)total.checks_failed,
            (unsigned long)(total.checks_passed + total.checks_failed),
            (unsigned long)total.suites_run);
    }

    return total;
}

// ── Manufacturing dispatch ────────────────────────────────────────────────────

auto lm::test::dispatch(u32 suite_hash, manufacturing_report& report) -> bool
{
    // Check manufacturing suites first.
    for(lm::st i = 0; i < mfg_count; ++i) {
        if(mfg_registry[i].hash != suite_hash) continue;
        mfg_registry[i].fn(report);
        return true;
    }

    // Fall back to unit suites — dispatch mechanism doubles as an integration
    // test runner. Wraps the unit result as measurements for wire compat.
    for(lm::st i = 0; i < unit_count; ++i) {
        if(unit_registry[i].hash != suite_hash) continue;

        auto r = unit_registry[i].fn();
        log_suite_result(r);

        if(r.failed == 0) {
            if(report.count < manufacturing_report::max_entries)
                report.entries[report.count++] = {
                    unit_registry[i].name, "checks",
                    (i64)r.passed, (i64)r.passed, true,
                };
            ++report.passed;
        } else {
            for(lm::st j = 0; j < r.failure_count; ++j) {
                if(report.count >= manufacturing_report::max_entries) break;
                report.entries[report.count++] = {
                    unit_registry[i].name, r.failures[j].expr,
                    0, 1, false,
                };
            }
            ++report.failed;
        }
        return true;
    }

    return false;
}
