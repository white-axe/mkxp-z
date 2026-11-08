parser = bison.generate(
    'parser.y',
    args: [
        '-Werror=conflicts-sr',
        '-Werror=conflicts-rr',
        '-Wcounterexamples',
    ],
    source: 'parser.cpp',
    header: 'parser.h',
)

lexer = flex.generate(
    'lexer.l',
    source: 'lexer.cpp',
)

global_include_dirs += include_directories([
    '.',
])

global_dependencies += declare_dependency(sources: [
    parser,
    lexer,
])
