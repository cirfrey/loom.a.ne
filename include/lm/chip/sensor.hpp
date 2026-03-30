#pragma once

#include "lm/core/types.hpp"

#include <optional>

namespace lm::chip::sensor
{
    // Returns the internal silicon temperature in Celsius.
    // Returns false if the hardware doesn't support this feature.
    /// TODO: f32 for now.
    auto internal_temperature() -> f32;
}
