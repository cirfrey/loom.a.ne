cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(LOOMANE_DEFS_PATH "$ENV{LOOMANE_DEFS_PATH}")
include("${LOOMANE_DEFS_PATH}")
project("loom.a.ne" VERSION "${LOOMANE_VERSION_MAJOR}.${LOOMANE_VERSION_MINOR}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/HEAD")

message("> Loom Audio Nexus v${LOOMANE_VERSION_MAJOR}.${LOOMANE_VERSION_MINOR} [${LOOMANE_GIT_HASH}:${LOOMANE_BUILD_DATE}]")
message("> Using BSP=[${LOOMANE_BOARD_TYPE}] HAL=[${LOOMANE_MCU_TYPE}]")

# Expose our tusb_config.h to tinyusb so it can find it.
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/lm/usbd/internal")
