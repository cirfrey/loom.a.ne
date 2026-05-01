generated_txt_headers = []
foreach f : ['banner', 'name']
    generated_txt_headers += custom_target(
        f + '_hpp',
        input : include/'lm/arch/x86_64' / f + '.txt',
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
