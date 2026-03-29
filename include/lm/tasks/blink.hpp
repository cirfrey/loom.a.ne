#pragma once

namespace lm::task { struct config; } // Forward decl.

namespace lm::blink
{
    auto task(lm::task::config const& cfg) -> void;
    auto init() -> void;
}
