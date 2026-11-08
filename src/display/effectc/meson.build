parser = custom_target(
    'parser.cpp',
    output: [
        'parser.cpp',
        'parser.h',
        'parser.output',
    ],
    command: [
        bison,
        '--color=' + get_option('b_colorout'),
        '-Werror=conflicts-sr',
        '-Werror=conflicts-rr',
        '-Wcounterexamples',
        '-v',
        '--header=@OUTPUT1@',
        '-o', '@OUTPUT0@',
        files('parser.y'),
    ],
)

lexer = custom_target(
    'lexer.cpp',
    output: [
        'lexer.cpp',
        'lexer.h',
    ],
    command: [
        flex,
        '--header-file=@OUTPUT1@',
        '-o', '@OUTPUT0@',
        files('lexer.l'),
    ],
)

global_include_dirs += include_directories([
    '.',
])

global_dependencies += declare_dependency(sources: [
    parser,
    lexer,
])
