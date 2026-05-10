
sdkconfig_h_content = '''
#pragma once
#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE
#define TUP_DCD_ENDPOINT_MAX 10
'''
# Inject our tusb_config.h and sdkconfig.h into tinyusb.
tusb_dep = declare_dependency(
    dependencies: [
        subproject('tinyusb', default_options: ['warning_level=0']).get_variable('tinyusb_dep').as_system(),
        [
            tusb_config,
            declare_dependency(
                sources: configure_file(
                    command: ['echo', sdkconfig_h_content],
                    capture: true,
                    output: 'sdkconfig.h',
                    build_subdir: 'generated/tusb_config',
                ),
                include_directories: include_directories('generated/tusb_config')
            )
        ],
    ]
)

subdir('include')

native_dep = declare_dependency(
    sources:
        loomane_sources +
        unit_test_runner +
        test_files +
        [
            dir_sources/'lm/arch/x86_64/chip/gpio.cpp',
            dir_sources/'lm/arch/x86_64/chip/info.cpp',
            dir_sources/'lm/arch/x86_64/chip/memory.cpp',
            dir_sources/'lm/arch/x86_64/chip/net.cpp',
            dir_sources/'lm/arch/x86_64/chip/sensor.cpp',
            dir_sources/'lm/arch/x86_64/chip/system.cpp',
            dir_sources/'lm/arch/x86_64/chip/time.cpp',
            dir_sources/'lm/arch/x86_64/chip/uart.cpp',
            dir_sources/'lm/arch/x86_64/chip/usb.cpp',
            dir_sources/'lm/arch/x86_64/fabric/primitives.cpp',
            dir_sources/'lm/arch/x86_64/fabric/strand.cpp',
            dir_sources/'lm/arch/x86_64/main/loomane.cpp',
            dir_sources/'lm/arch/x86_64/arch_hooks.cpp',
            dir_sources/'lm/arch/x86_64/program_args.cpp',
            dir_sources/'lm/arch/x86_64/tusb.cpp',
        ]
    ,
    include_directories: include_directory,
    dependencies: [
        build_hpp,
        board_hpp,
        generated_txt_headers,
        tusb_dep,
        dependency('libusb', default_options: ['warning_level=0'], include_type: 'system'),
    ]
)

executables += {
    'target_name': 'loomane',
    'dependencies': native_dep,
}
