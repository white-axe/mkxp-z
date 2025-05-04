project('theoraplay', 'c', meson_version: '>=1.3.0')

theoraplay = declare_dependency(
    include_directories: '.',
    link_with: library(
        'theoraplay',
        dependencies: get_option('use_shim') ? subproject('theoraplay-shim').get_variable('theoraplay_shim') : [],
        c_args: get_option('cflags'),
        sources: [
            'theoraplay.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)
