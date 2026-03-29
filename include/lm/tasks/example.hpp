// Use this as a starting point for your task.
// Don't forget to get a unique id and config in lm::config::task.
#pragma once

namespace lm::task { struct config; } // Forward decl.

namespace lm::example
{
    auto task(lm::task::config const& cfg) -> void;
    auto init() -> void;
}
