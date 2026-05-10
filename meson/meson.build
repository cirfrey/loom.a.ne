unit_test_runner = files(
    dir_sources/'lm/test/unit.cpp'
)
test_files = files(
    dir_sources/'lm/core/primitives/bitset.test.cpp',
    dir_sources/'lm/core/endian.test.cpp',
    dir_sources/'lm/fabric/bus.test.cpp',
    dir_sources/'lm/fabric/event.test.cpp',
    dir_sources/'lm/lib/midi.test.cpp',
    dir_sources/'lm/ini.multikey.test.cpp',
    dir_sources/'lm/ini.test.cpp',
)
# Everything except arch and testing files
loomane_sources = files(
    dir_sources/'lm/fabric/bus.cpp',
    dir_sources/'lm/fabric/strand.cpp',
    dir_sources/'lm/fabric/utils.cpp',
    dir_sources/'lm/strands/usb/transport/usbip.cpp',
    dir_sources/'lm/strands/usb/controller.cpp',
    dir_sources/'lm/strands/blink.cpp',
    dir_sources/'lm/strands/busmon.cpp',
    dir_sources/'lm/strands/example.cpp',
    dir_sources/'lm/strands/healthmon.cpp',
    dir_sources/'lm/strands/log.cpp',
    dir_sources/'lm/strands/strandman.cpp',
    dir_sources/'lm/strands/usbd.cpp',
    dir_sources/'lm/usb/common.cpp',
    dir_sources/'lm/usbd/dcd.cpp',
    dir_sources/'lm/usbd/descriptors.cpp',
    dir_sources/'lm/banner.cpp',
    dir_sources/'lm/config.cpp',
    dir_sources/'lm/hooks.cpp',
    dir_sources/'lm/ini.cpp',
    dir_sources/'lm/log.cpp',
    dir_sources/'lm/registry.cpp',
    dir_sources/'lm/test.cpp',
)

# Defines board, arch and others, used by the generator.
subdir('environment/'/environment/'variables')
subdir('generator')
subdir('environment'/environment)
