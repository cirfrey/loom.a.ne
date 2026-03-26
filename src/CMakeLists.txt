# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

list(APPEND app_sources
    "${CMAKE_SOURCE_DIR}/src/main.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/bus.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/task.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/blink.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/logging.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/cdc.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/hid.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/midi.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/msc.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/uac2.cpp"
    "${CMAKE_SOURCE_DIR}/src/espy/usb/usb.cpp"
)

idf_component_register(SRCS ${app_sources})

#target_sources(${tusb_lib} PUBLIC "${COMPONENT_DIR}/tusb/usb_descriptors.c")
