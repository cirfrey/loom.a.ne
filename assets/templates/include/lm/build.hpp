#pragma once

#include "lm/core/types.hpp"

namespace lm::build {
    static constexpr u8    version_major = @PROJECT_VERSION_MAJOR@;
    static constexpr u8    version_minor = @PROJECT_VERSION_MINOR@;

    static constexpr text  git_hash   = "@GIT_HASH@"_text;
    static constexpr text  build_date = "@BUILD_DATE@"_text;

    static constexpr text  board = "@BOARD@"_text;
    static constexpr text  mcu   = "@MCU@"_text;
    static constexpr text  arch  = "@MCU@"_text;
}
