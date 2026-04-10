#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::info
{
    // Internal, for funsies.
    auto codename() -> text;
    // Most likely what's going to be used in the boot banner.
    auto name() -> text;
    auto uuid() -> text;

    // Custom banner art per-chip for funsies.
    // There's a default one, if you don't want to customize it
    // just set this to .data=nullptr, .len=0.
    auto banner() -> text;
}
