parser = custom_target(
    'parser.cpp',
    output: [
        'parser.cpp',
        'parser.hpp',
        'parser.output',
    ],
    command: [
        bison,
        '-v',
        '-Werror=conflicts-sr',
        '-Werror=conflicts-rr',
        '-Wcounterexamples',
        '-o', '@OUTPUT0@',
        files('parser.y'),
    ],
)

lexer = custom_target(
    'lexer.cpp',
    output: 'lexer.cpp',
    command: [
        flex,
        '-o', '@OUTPUT@',
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

global_sources += [
    parser,
    lexer,
]
