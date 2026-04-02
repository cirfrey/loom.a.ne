#pragma once

namespace lm::task { struct config; } // Forward decl.

// The system manager.
namespace lm::sysman
{
    auto task(lm::task::config const& cfg) -> void;
    auto init() -> void;
}
