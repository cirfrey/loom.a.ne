#pragma once

namespace espy::logging
{
    auto init() -> void;

    auto logf(const char *fmt, ...) -> int;
}
