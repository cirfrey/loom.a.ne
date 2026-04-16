#pragma once

// loomane_entrypoint is only the launcher to the application, their responsibilities are
// - Init the hardware.
// - Let sysman init the software and handle all the strand orchestration.
// It should not:
// - Run application code, that would be the lm::app::strand.
//
// Do NOT assume this function is [[noreturn]], it *does* return.
//
// There's a default implementation in src/lm/entrypoint.cpp.
// Feel free to override it if you need.
extern "C" [[gnu::weak]] auto loomane_entrypoint() -> void;
extern "C" auto loomane_default_entrypoint() -> void;
