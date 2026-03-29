#include "lm/board.hpp"

#include "lm/version/defs.hpp"
#include "lm/version/banner.hpp"
#include "lm/task.hpp"
#include "lm/tasks/sysman.hpp"

// app_main is only the entrypoint/launcher to the application, their responsibilities are
// - Init the hardware.
// - Let sysman init the software and handle all the task orchestration.
// It should not:
// - Run application code, that would be the lm::app::task.
//
// NOTE: app_main is special and doesn't need to be terminated (vTaskDelete),
//       whoever spawned it will take care of the cleanup.
extern "C" auto app_main() -> void
{
    lm::board::init_console();
    lm::board::init_hardware();

    // By printing the banner we make sure that things like the mac address are
    // initialized and can be used by the rest of the code.
    lm::version::write_banner_to_console(
        lm::version::major, lm::version::minor,
        lm::version::git_hash, lm::version::build_date
    );
    lm::task::delay_ms(500); // Take a moment to appreciate the banner, it's pretty.

    lm::sysman::init();
}
