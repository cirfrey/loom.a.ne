// Unit test hook — provides lm::hook::test::unit() which is declared [[gnu::weak]].
// Compiled only in test envs (build_src_filter). In production builds the weak
// default (nullptr) is used and the launcher skips this entirely.

#include "lm/hooks.hpp"
#include "lm/test.hpp"
#include "lm/config.hpp"
#include "lm/log.hpp"
#include "lm/chip/system.hpp"

#include "lm/board.hpp"

auto lm::hook::test::unit(config_t& config) -> void
{
    auto const saved_dispatcher = config.logging.custom_dispatcher;
    config.logging.custom_dispatcher = [](text t, log::level) -> bool {
        // dispatch_immediate writes directly to UART without the ring buffer.
        // Port 0 = stdout on native, UART0 on embedded.
        lm::log::dispatch_immediate(
            board::uart_unit_test,
            t,
            100'000,   // 100ms timeout — generous, we're pre-scheduler
            false      // don't yield — scheduler may not be running
        );
        return true;
    };
    auto const result = lm::test::run_unit();
    config.logging.custom_dispatcher = saved_dispatcher;

    using after_t = config_t::test_t::after_test;
    switch(lm::config.test.after_unit) {
        case after_t::proceed: return;

        case after_t::proceed_if_ok:
            if(result.ok()) return;
            // Failures were logged above. Fall through to halt.
            log::panic("Unit tests failed — halting (test.after_unit = proceed_if_ok)\n");
            chip::system::halt(1);
            break;

        case after_t::halt:
            if(result.ok()) {
                lm::log::info("Unit tests passed — halting (test.after_unit = halt)\n");
                chip::system::halt(0);
            }
            else {
                lm::log::warn("Unit tests failed — halting (test.after_unit = halt)\n");
                chip::system::halt(1);
            }

        default:
            log::panic("Unhandled setting for lm::config.launcher.test.after_unit %u - halting", lm::config.test.after_unit);
            chip::system::halt(1);
    }

}
