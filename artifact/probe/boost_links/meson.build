project('boost_links', 'cpp', version: '1.0.0')

executable(
    'test_exe',
    'boost_links.cpp',
    dependencies: dependency(
        'boost',
        modules: ['fiber', 'context'],
        required: true
    )
)
