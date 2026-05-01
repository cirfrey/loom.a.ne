# Inject our tusb_config.h into tinyusb.

sdkconfig_h = '''
#pragma once
#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE
#define TUP_DCD_ENDPOINT_MAX 10
'''
sdk_config = declare_dependency(
    sources: [
        configure_file(
            command: ['echo', sdkconfig_h],
            capture: true,
            output: 'sdkconfig.h',
            build_subdir: 'generated/tusb_config',
        )
    ],
    include_directories: include_directories('generated/tusb_config')
)

tusb_dep = declare_dependency(
    dependencies: [
        subproject('tinyusb').get_variable('tinyusb_dep').as_system(),
        [tusb_config, sdk_config],
    ]
)

subdir('include')

executable(
    'loomane',
    sources:
        loomane_sources +
        unit_test_runner +
        test_files +
        [
            src/'lm/arch/x86_64/chip/gpio.cpp',
            src/'lm/arch/x86_64/chip/info.cpp',
            src/'lm/arch/x86_64/chip/memory.cpp',
            src/'lm/arch/x86_64/chip/net.cpp',
            src/'lm/arch/x86_64/chip/sensor.cpp',
            src/'lm/arch/x86_64/chip/system.cpp',
            src/'lm/arch/x86_64/chip/time.cpp',
            src/'lm/arch/x86_64/chip/uart.cpp',
            src/'lm/arch/x86_64/chip/usb.cpp',
            src/'lm/arch/x86_64/fabric/primitives.cpp',
            src/'lm/arch/x86_64/fabric/strand.cpp',
            src/'lm/arch/x86_64/main/loomane.cpp',
            src/'lm/arch/x86_64/arch_hooks.cpp',
            src/'lm/arch/x86_64/program_args.cpp',
            src/'lm/arch/x86_64/tusb.cpp',
        ]
    ,
    include_directories: incdirs,
    dependencies: [
        generated_files,
        generated_txt_headers,
        tusb_dep,
        dependency('libusb', include_type: 'system'),
    ]
)
