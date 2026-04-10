
list(APPEND app_sources
    "main.cpp"

    "lm/log.cpp"

    "lm/arch/${LOOMANE_MCU_TYPE}/hal.cpp"
    "lm/arch/${LOOMANE_MCU_TYPE}/osal.cpp"
    "lm/arch/${LOOMANE_MCU_TYPE}/dcd_dwc2.cpp"

    "lm/fabric/bus.cpp"
    "lm/fabric/task.cpp"

    "lm/tasks/example.cpp"
    "lm/tasks/blink.cpp"
    "lm/tasks/busmon.cpp"
    "lm/tasks/healthmon.cpp"
    "lm/tasks/usbd.cpp"
    "lm/tasks/tman.cpp"
    "lm/tasks/log.cpp"

    "lm/usbd/cdc.cpp"
    "lm/usbd/dcd_usbip.cpp"
    "lm/usbd/dcd.cpp"
    "lm/usbd/hid.cpp"
    "lm/usbd/midi.cpp"
    "lm/usbd/msc.cpp"
    "lm/usbd/uac2.cpp"
    "lm/usbd/usbd.cpp"

    "lm/version/banner.cpp"
)

idf_component_register(
    SRCS ${app_sources} ${LOOMANE_TINYUSB_SOURCES}
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/include/lm/board/${LOOMANE_BOARD_TYPE}"
)
