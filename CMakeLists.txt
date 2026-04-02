cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("loom.a.ne" VERSION 0.2)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/HEAD")

# Generate lm/version/defs.hpp.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()
string(TIMESTAMP BUILD_DATE "%Y-%m-%dT%H:%M:%S")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/lm/version/defs.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include/lm/version/defs.hpp"
    @ONLY
)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/generated/include")
message("> Loom Audio Nexus v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR} [${GIT_HASH}:${BUILD_DATE}]")

fatfs_create_spiflash_image(static assets/static FLASH_IN_PROJECT)

# Expose our tusb_config.h to tinyusb so it can find it.
idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/lm/usbd/internal")
