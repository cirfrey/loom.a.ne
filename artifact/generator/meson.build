git_hash = run_command('git', 'rev-parse', '--short', 'HEAD', check: true).stdout().strip()
version_major = meson.project_version().split('.')[0]
version_minor = meson.project_version().split('.')[1]
current_datetime = run_command(python, '-c', 'import datetime; print(datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"))', check: true).stdout().strip()

conf_data = {
    'PROJECT_VERSION_MAJOR': version_major,
    'PROJECT_VERSION_MINOR': version_minor,
    'GIT_HASH':              git_hash,
    'BUILD_DATE':            current_datetime,
    'BOARD':                 board,
    'MCU':                   arch,
    'MAX_RAM':               '0',
}

banner_defs_ini = configure_file(
    input: 'templates/banner_defs.ini',
    output: 'banner_defs.ini',
    build_subdir: 'generated',
    configuration: conf_data,
)

generated_files = declare_dependency(
    sources: [
        configure_file(
            input: 'templates/include/lm/build.hpp',
            output: 'build.hpp',
            build_subdir: 'generated/include/lm',
            configuration: conf_data,
        ),
        configure_file(
            input: include/'lm/board'/board/'board.hpp',
            output: 'board.hpp',
            build_subdir: 'generated/include/lm',
            copy: true,
        )
    ],
    include_directories: include_directories('generated/include')
)

tusb_config = declare_dependency(
    sources: [
        configure_file(
            input: include/'lm'/'usbd'/'tusb_config.h',
            output: 'tusb_config.h',
            build_subdir: 'generated/tusb_config',
            copy: true,
        )
    ],
    include_directories: include_directories('generated/tusb_config')
)

txt2hpp = files('txt2hpp.py')
