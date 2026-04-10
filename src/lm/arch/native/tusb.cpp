#include "lm/core/types.hpp"
#include "lm/chip/time.hpp"
#include "lm/fabric/task.hpp"

extern "C" lm::u32 tusb_time_millis_api(void) {
    return lm::chip::time::uptime() / 1000;
}

extern "C" void tusb_time_delay_ms_api(lm::u32 ms) {
    lm::fabric::task::sleep_ms(ms);
}