project(
    'loom.a.ne',
    'c', 'cpp',
    version: '0.8',
    meson_version: '>= 1.10.0',
    default_options: [ 'warning_level=3', 'cpp_std=c++20' ]
)

python = import('python').find_installation('python3')

environment = meson.get_external_property('environment', 'native')
board       = meson.get_external_property('board',       'native')
arch        = meson.get_external_property('arch',        'x86_64')

src = meson.current_source_dir()/'..'/'src'
include = meson.current_source_dir()/'..'/'include'
incdirs = include_directories('../include')

subdir('generator')

unit_test_runner = files(
    src/'lm/test/unit.cpp'
)
test_files = files(
    src/'lm/core/primitives/bitset.test.cpp',
    src/'lm/core/endian.test.cpp',
    src/'lm/fabric/bus.test.cpp',
    src/'lm/fabric/event.test.cpp',
    src/'lm/lib/midi.test.cpp',
    src/'lm/ini.multikey.test.cpp',
    src/'lm/ini.test.cpp',
)
# Everything except arch and testing files
loomane_sources = files(
    src/'lm/fabric/bus.cpp',
    src/'lm/fabric/strand.cpp',
    src/'lm/fabric/utils.cpp',
    src/'lm/strands/usb/transport/usbip.cpp',
    src/'lm/strands/usb/controller.cpp',
    src/'lm/strands/blink.cpp',
    src/'lm/strands/busmon.cpp',
    src/'lm/strands/example.cpp',
    src/'lm/strands/healthmon.cpp',
    src/'lm/strands/log.cpp',
    src/'lm/strands/strandman.cpp',
    src/'lm/strands/usbd.cpp',
    src/'lm/usb/common.cpp',
    src/'lm/usbd/dcd.cpp',
    src/'lm/usbd/descriptors.cpp',
    src/'lm/banner.cpp',
    src/'lm/config.cpp',
    src/'lm/hooks.cpp',
    src/'lm/ini.cpp',
    src/'lm/log.cpp',
    src/'lm/registry.cpp',
    src/'lm/test.cpp',
)

# Run the meson.build of the environment.
subdir('environment'/environment)
