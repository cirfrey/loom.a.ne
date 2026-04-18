#pragma once

#include "lm/core/types.hpp"

namespace lm::chip::info
{
    // Internal, for funsies.
    auto codename() -> text;
    // Most likely what's going to be used in the boot banner.
    auto name() -> text;
    // Does *not* have to be a MAC address, it just happens to be
    // one sometimes, it is silly like.
    // This is just an id string, nothing special my comrade.
    auto uuid() -> text;

    // Custom banner art per-chip for funsies.
    // There's a default one, if you don't want to customize it
    // just set this to .data=nullptr, .len=0.
    auto banner() -> text;
}
