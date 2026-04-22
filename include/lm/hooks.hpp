// Hooks for the loom.a.ne framework.
#pragma once

// Forward decl.
namespace lm { struct config_t; }

// The following functions are declared in the order they are called.
// The ones marked [WEAK] can be overriden by you.
// The ones prefixed by arch_* are implemented by the underlying architecture.
namespace lm::hook
{
    // The standard loom.a.ne launcher, calls all the functions below. If you override with your own make
    // sure to do things in the proper order.
    // Do NOT assume this function is [[noreturn]], it *does* return (depends on your main() really).
    // TODO: allow override.
    /* [WEAK] */ auto launcher() -> void;


    auto framework_init(config_t&) -> void;
    auto arch_init(config_t&) -> void;
    /* [WEAK] */ auto init(config_t&) -> void;


    auto arch_config(config_t&) -> void;
    /* [WEAK] */ auto config(config_t&) -> void;


    auto parse_ini(config_t&) -> void;


    namespace test
    {
        // When compiled with unit tests, this is linked. Only runs if compiled AND
        // config.test.unit == feature::on. You could override this if you want but you
        // really should just integrate your tests into the test runner.
        /* [WEAK] */ auto unit(config_t&) -> void;
    }


    auto framework_main() -> void;
    /* [WEAK] */ auto main() -> void;
}
