generated_txt_headers = []
foreach f : ['banner', 'name']
    generated_txt_headers += custom_target(
        f + '_hpp',
        input : dir_include/'lm/arch/x86_64' / f + '.txt',
        output : f + '.hpp',
        build_subdir: 'lm/arch/x86_64',
        capture: true,
        command : [python, txt2hpp, '@INPUT@'],
    )
endforeach
generated_txt_headers = declare_dependency(
    sources: generated_txt_headers,
    include_directories: include_directories('.'),
)

board_hpp = declare_dependency(
    sources: configure_file(
        input: dir_include/'lm/board/native/board.hpp',
        output: 'board.hpp',
        build_subdir: 'lm',
        copy: true,
    ),
    include_directories: include_directories('.')
)
