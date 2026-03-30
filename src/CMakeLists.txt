
list(APPEND app_sources
    "main.cpp"

    "lm/bus.cpp"
    "lm/task.cpp"
    "lm/version/banner.cpp"

    ### TODO: configurable.
    "lm/chips/esp32.cpp"

    "lm/tasks/app.cpp"
    "lm/tasks/blink.cpp"
    "lm/tasks/busmon.cpp"
    #"lm/tasks/example.cpp"
    "lm/tasks/healthmon.cpp"
    "lm/tasks/logging.cpp"
    "lm/tasks/sysman.cpp"
    "lm/tasks/usbd.cpp"

    "lm/usbd/cdc.cpp"
    "lm/usbd/hid.cpp"
    "lm/usbd/midi.cpp"
    "lm/usbd/msc.cpp"
    "lm/usbd/uac2.cpp"
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include/lm/boards/${LOOMANE_BOARD_TYPE}"
)
