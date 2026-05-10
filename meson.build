project(
    'loom.a.ne',
    'c', 'cpp',
    version: '0.8',
    meson_version: '>= 1.10.0',
    default_options: [ 'warning_level=3', 'cpp_std=c++20' ]
)

python = import('python').find_installation('python3')

environment = get_option('environment')

dir_sources = meson.current_source_dir()/'src'
dir_include = meson.current_source_dir()/'include'
dir_scripts = meson.current_source_dir()/'scripts'
dir_build   = meson.current_build_dir()

include_directory = include_directories('include')

# Build executables at the root level for easier access.
executables = []
subdir('meson')
foreach e : executables
    executable(
        e['target_name'],
        sources:             e.get('sources', []),
        dependencies:        e.get('dependencies', []),
        include_directories: e.get('include_directories', []),
        c_args:              e.get('c_args', []),
        cpp_args:            e.get('cpp_args', []),
        link_args:           e.get('link_args', []),
        install:             e.get('install', false),
        build_by_default:    e.get('build_by_default', true),
    )
endforeach
