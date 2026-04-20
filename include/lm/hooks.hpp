#pragma once

// Hooks for the loom.a.ne framework.

// The following functions are declared in the order they are called.
// The ones marked [WEAK] can be overriden by you.
// The ones prefixed by arch_* are implemented by the underlying architecture.
namespace lm::hook
{
    // The standard loom.a.ne launcher, calls all the functions below. If you override with your own make
    // sure to do things in the proper order.
    // Do NOT assume this function is [[noreturn]], it *does* return (depends on your main() really).
    // [WEAK].
    // TODO: allow override.
    auto launcher() -> void;

    auto framework_init() -> void;
    auto arch_init() -> void;
    // [WEAK].
    auto init() -> void;

    namespace test
    {
        // When compiled with unit tests, this is linked. Only runs if compiled AND
        // config.test.unit == feature::on. You could override this if you want but you
        // really should just integrate your tests into the test runner.
        // [WEAK].
        auto unit() -> void;
    }

    auto arch_config() -> void;
    // [WEAK].
    auto config() -> void;

    auto framework_main() -> void;
    // [WEAK].
    auto main() -> void;
}
