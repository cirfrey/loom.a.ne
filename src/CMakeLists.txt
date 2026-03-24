# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

list(APPEND app_sources
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/blink.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/usb.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/common.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/logging.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/hid.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/midi.cpp"
)

idf_component_register(SRCS ${app_sources})

#target_sources(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb/usb_descriptors.c")
