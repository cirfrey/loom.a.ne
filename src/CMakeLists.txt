
list(APPEND app_sources
    "main.cpp"

    "lm/log.cpp"

    "lm/fabric/bus.cpp"
    "lm/fabric/task.cpp"

    "lm/version/banner.cpp"

    "lm/arch/${LOOMANE_MCU_TYPE}/hal.cpp"
    "lm/arch/${LOOMANE_MCU_TYPE}/osal.cpp"

    "lm/tasks/example.cpp"
    "lm/tasks/blink.cpp"
    "lm/tasks/busmon.cpp"
    "lm/tasks/healthmon.cpp"
    "lm/tasks/usbd.cpp"
    "lm/tasks/tman.cpp"
    "lm/tasks/log.cpp"

    "lm/usbd/cdc.cpp"
    "lm/usbd/hid.cpp"
    "lm/usbd/midi.cpp"
    "lm/usbd/msc.cpp"
    "lm/usbd/uac2.cpp"
    "lm/usbd/usbd.cpp"
)

idf_component_register(
    SRCS ${app_sources} ${LOOMANE_TINYUSB_SOURCES}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include/lm/board/${LOOMANE_BOARD_TYPE}"
)


# Define the generator executable
add_executable(gen_build_info lm/gen_build_info.cpp)
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/generated/build_info.txt"
    COMMAND gen_build_info > "${CMAKE_BINARY_DIR}/generated/build_info.txt"
    DEPENDS gen_build_info
    COMMENT "Generating build info artifact via printf redirection"
)
# Create a top-level target you can call from Python
add_custom_target(gen_build_info_tgt ALL DEPENDS "${CMAKE_BINARY_DIR}/generated/build_info.txt")
