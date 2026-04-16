#pragma once

// Hooks for the loom.a.ne framework.

namespace lm::entrypoint
{
    // The standard loom.a.ne launcher.
    // Do NOT assume this function is [[noreturn]], it *does* return.
    auto launcher() -> void;
    // You can override it by setting this.
    [[gnu::weak]] auto launcher_override() -> void;

    // The following are implemented by the architecture.
    auto arch_init() -> void;
    auto arch_config() -> void;
}
