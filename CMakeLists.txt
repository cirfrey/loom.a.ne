cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(ESPY-Test-TUSB)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/espy/usb/internal")
target_compile_options(${tusb_lib} PRIVATE "-Wno-deprecated-volatile")
