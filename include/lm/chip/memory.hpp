#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::memory
{
    // How much RAM there is in total in bytes.
    auto total() -> st;
    // Available RAM (heap) in bytes.
    auto free() -> st;
    // Get the maxiumum amount of RAM that's been used (high water mark) in bytes.
    auto peak_used() -> st;
    // Get the size of the largest free block in RAM in bytes.
    auto largest_free_block() -> st;
}

/// Gemini:
/// (Future) You will likely need to distinguish between internal RAM and
/// external PSRAM for audio buffers: auto free_internal() -> st; and auto free_psram() -> st;
