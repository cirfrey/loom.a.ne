#include "lm/chip/all.hpp"
#include "lm/core/all.hpp"

auto lm::chip::usb::phy::power_up() -> void
{
    // No-op. TinyUSB Native port usually hooks directly into the host OS networking/USB stack
    // or runs via an emulation layer without needing raw PHY config.
}
