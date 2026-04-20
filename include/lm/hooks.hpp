#pragma once

// Hooks for the loom.a.ne framework.

// The following functions are declared in the order they are called.
// The ones marked [[gnu::weak]] can be overriden by you.
// The ones prefixed by arch_* are implemented by the underlying architecture.
namespace lm::hook
{
    // The standard loom.a.ne launcher.
    // Do NOT assume this function is [[noreturn]], it *does* return (depends on your main() really).
    auto launcher() -> void;
    // You can override it by setting this.
    [[gnu::weak]] auto launcher_override() -> void;

    auto arch_init() -> void;
    [[gnu::weak]] auto init() -> void;

    namespace test
    {
        // When compiled with unit tests, this is linked. Only runs if compiled AND
        // config.test.unit == feature::on. You could override this if you want but you
        // really should just integrate your tests into the test runner.
        [[gnu::weak]] auto unit() -> void;
    }

    auto arch_config() -> void;
    [[gnu::weak]] auto config() -> void;

    [[gnu::weak]] auto main() -> void;
}
