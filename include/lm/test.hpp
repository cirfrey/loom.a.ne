#pragma once

// ┌─────────────────────────────────────────────────────────────────────────┐
// │  lm::test — integrated testing framework                                │
// │                                                                         │
// │  Three modes:                                                           │
// │    unit         — pre-system, synchronous. Runs via hook::test::unit(). │
// │    integration  — event-driven alongside live system (testing_strand).  │
// │    manufacturing — demand-driven from jig via testing_strand.           │
// │                                                                         │
// │  User API: LM_TEST_SUITE and LM_MFG_SUITE macros only.                 │
// └─────────────────────────────────────────────────────────────────────────┘

#include "lm/core/types.hpp"

namespace lm::test
{
    // ── Result types ─────────────────────────────────────────────────────────

    struct check_result
    {
        bool        passed  = false;
        const char* expr    = nullptr;   // expression string from call site
        const char* file    = nullptr;   // __builtin_FILE()
        u32         line    = 0;         // __builtin_LINE()
        const char* context = nullptr;   // optional, nullptr if unused
    };

    struct suite_result
    {
        const char* name          = nullptr;
        u32         passed        = 0;
        u32         failed        = 0;

        static constexpr st max_failures = 16;
        check_result failures[max_failures] = {};
        u32          failure_count          = 0;
    };

    struct run_result
    {
        u32 suites_run    = 0;
        u32 checks_passed = 0;
        u32 checks_failed = 0;

        auto ok() const -> bool { return checks_failed == 0; }
    };

    // ── Manufacturing types ───────────────────────────────────────────────────

    struct measurement
    {
        const char* subsystem = nullptr;
        const char* metric    = nullptr;
        i64         value     = 0;      // actual measured value
        i64         budget    = 0;      // pass if |value| <= budget
        bool        pass      = false;
    };

    struct manufacturing_report
    {
        static constexpr st max_entries = 32;
        measurement entries[max_entries] = {};
        u32         count  = 0;
        u32         passed = 0;
        u32         failed = 0;
    };

    // ── Suite function types ──────────────────────────────────────────────────

    using unit_suite_fn = suite_result (*)();
    using mfg_suite_fn  = void (*)(manufacturing_report&);

    // ── Registration — called by macros at static init, not user code ─────────

    auto register_unit_suite(const char* name, unit_suite_fn) -> void;
    auto register_mfg_suite (const char* name, mfg_suite_fn)  -> void;

    // ── Runners — called by the framework, not user code ─────────────────────

    auto run_unit()                                      -> run_result;
    auto dispatch(u32 suite_hash, manufacturing_report&) -> bool;
}

// ── Macro helpers ─────────────────────────────────────────────────────────────

#ifndef LM_TEST_CONCAT
#define LM_TEST_CONCAT_IMPL_(a_, b_) a_##b_
#define LM_TEST_CONCAT(a_, b_) LM_TEST_CONCAT_IMPL_(a_, b_)
#endif

// ── LM_TEST_SUITE ─────────────────────────────────────────────────────────────
//
// Registers a unit test suite. Available inside the block:
//   check(cond, "what you're checking")
//   check_ctx(cond, "what you're checking", "extra context on failure")
//
// __builtin_LINE() / __builtin_FILE() as default parameters capture the
// call site — this is the same trick as std::source_location but works on
// GCC/Clang back to C++11 and on Xtensa.
//
// Suite names follow "module.subject" convention:
//   "ini.number", "bus.subscribe", "usbd.midi_packet"
//
// Example:
//   LM_TEST_SUITE("ini.number",
//   {
//       lm::u32 out = 0;
//       auto f = lm::ini::number_field("x"_text, out, {});
//       check(f.parse("42") == lm::ini::parse_result::ok, "decimal parse ok");
//       check(out == 42u, "decimal value correct");
//   });

#define LM_TEST_SUITE(name_, ...)                                               \
    static auto LM_TEST_CONCAT(_lm_suite_fn_, __LINE__)()                            \
        -> lm::test::suite_result                                               \
    {                                                                           \
        lm::test::suite_result _r;                                              \
        _r.name = name_;                                                        \
                                                                                \
        auto check = [&](                                                       \
            bool        _cond,                                                  \
            const char* _expr,                                                  \
            lm::u32     _line = static_cast<lm::u32>(__builtin_LINE()),         \
            const char* _file = __builtin_FILE()                                \
        ) {                                                                     \
            if(_cond) { ++_r.passed; return; }                                  \
            ++_r.failed;                                                        \
            if(_r.failure_count < lm::test::suite_result::max_failures)         \
                _r.failures[_r.failure_count++] = {                             \
                    false, _expr, _file, _line, nullptr};                       \
        };                                                                      \
                                                                                \
        auto check_ctx = [&](                                                   \
            bool        _cond,                                                  \
            const char* _expr,                                                  \
            const char* _ctx,                                                   \
            lm::u32     _line = static_cast<lm::u32>(__builtin_LINE()),         \
            const char* _file = __builtin_FILE()                                \
        ) {                                                                     \
            if(_cond) { ++_r.passed; return; }                                  \
            ++_r.failed;                                                        \
            if(_r.failure_count < lm::test::suite_result::max_failures)         \
                _r.failures[_r.failure_count++] = {                             \
                    false, _expr, _file, _line, _ctx};                          \
        };                                                                      \
                                                                                \
        (void)check_ctx;                                                        \
        __VA_ARGS__                                                              \
        return _r;                                                              \
    };                                                                          \
    static int LM_TEST_CONCAT(_lm_suite_reg_, __LINE__) =                            \
        (lm::test::register_unit_suite(                                         \
            name_, LM_TEST_CONCAT(_lm_suite_fn_, __LINE__)), 0)

// ── LM_MFG_SUITE ──────────────────────────────────────────────────────────────
//
// Registers a manufacturing suite. Available inside the block:
//   measure("subsystem", "metric", value, budget)
//   Passes if |value| <= budget.
//
// Example:
//   LM_MFG_SUITE("module_v1",
//   {
//       measure("usb",   "enumeration_ms",  chip::usb::enumeration_time_ms(), 500);
//       measure("audio", "clock_jitter_us", audio::measure_clock_jitter_us(100), 100);
//   });

#define LM_MFG_SUITE(name_, ...)                                                \
    static auto LM_TEST_CONCAT(_lm_mfg_fn_, __LINE__)                               \
        (lm::test::manufacturing_report& _report) -> void                       \
    {                                                                           \
        auto measure = [&](                                                     \
            const char* _sys,                                                   \
            const char* _metric,                                                \
            lm::i64     _val,                                                   \
            lm::i64     _budget                                                 \
        ) {                                                                     \
            if(_report.count >= lm::test::manufacturing_report::max_entries)    \
                return;                                                         \
            bool _pass = (_val < 0 ? -_val : _val) <= _budget;                  \
            _report.entries[_report.count++] = {                                \
                _sys, _metric, _val, _budget, _pass};                           \
            if(_pass) ++_report.passed; else ++_report.failed;                  \
        };                                                                      \
        __VA_ARGS__                                                             \
    };                                                                          \
    static int LM_TEST_CONCAT(_lm_mfg_reg_, __LINE__) =                             \
        (lm::test::register_mfg_suite(                                          \
            name_, LM_TEST_CONCAT(_lm_mfg_fn_, __LINE__)), 0)
