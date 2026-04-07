cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(LOOMANE_DEFS_PATH "$ENV{LOOMANE_DEFS_PATH}")
include("${LOOMANE_DEFS_PATH}")
project("loom.a.ne" VERSION "${LOOMANE_VERSION_MAJOR}.${LOOMANE_VERSION_MINOR}")

message("> Loom Audio Nexus v${LOOMANE_VERSION_MAJOR}.${LOOMANE_VERSION_MINOR} [${LOOMANE_GIT_HASH}:${LOOMANE_BUILD_DATE}]")
message("> Using BSP=[${LOOMANE_BOARD_TYPE}] HAL=[${LOOMANE_MCU_TYPE}]")
