project('libidn', 'c', meson_version: '>=1.3.0')

configuration = {
    'HAVE_STDBOOL_H': 1,
    'HAVE_VISIBILITY': meson.get_compiler('c').compiles('__attribute__((__visibility__("default"))) void f(void) {}', name: 'GNU visibility attribute support check') ? 1 : 0,
    'ICONV_CONST': '',
}

libidn = declare_dependency(
    include_directories: '.',
    link_with: library(
        'idn',
        dependencies: declare_dependency(
            sources: [
                configure_file(
                    input: 'config.h.in',
                    output: 'config.h',
                    configuration: configuration,
                ),
            ],
        ),
        c_args: get_option('cflags') + ['-DLIBIDN_BUILDING'],
        include_directories: 'lib',
        sources: [
            'lib/punycode.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)
