#include "lm/entrypoint.hpp"

// NOTE: app_main is special and doesn't need to be terminated (vTaskDelete),
//       whoever spawned it will take care of the cleanup.
extern "C" auto app_main() -> void
{
    if(loomane_entrypoint) loomane_entrypoint();
    else                   loomane_default_entrypoint();
}
