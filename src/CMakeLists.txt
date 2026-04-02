
list(APPEND app_sources
    "main.cpp"

    "lm/log.cpp"

    "lm/fabric/bus.cpp"
    "lm/fabric/task.cpp"

    "lm/version/banner.cpp"

    ### TODO: configurable.
    "lm/arch/esp32/hal.cpp"
    "lm/arch/esp32/osal.cpp"

    # Template for new tasks.
    #"lm/tasks/example.cpp"

    "lm/tasks/blink.cpp"
    "lm/tasks/busmon.cpp"
    "lm/tasks/healthmon.cpp"
    "lm/tasks/usbd.cpp"
    # Disabled for now.
    #"lm/tasks/tman.cpp"
    "lm/tasks/log.cpp"

    "lm/usbd/cdc.cpp"
    "lm/usbd/hid.cpp"
    "lm/usbd/midi.cpp"
    "lm/usbd/msc.cpp"
    "lm/usbd/uac2.cpp"
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include/lm/board/${LOOMANE_BOARD_TYPE}"
)
