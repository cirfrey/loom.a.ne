#include "lm/chip/all.hpp"
#include "lm/core/all.hpp"

auto lm::chip::sensor::internal_temperature() -> f32
{
    // Desktop CPUs run hot; here's a warm mock value
    return 45.5f;
}
