project('libffi', 'c', meson_version: '>=1.2.0')

cflags = []

if get_option('b_lto')
    if get_option('b_lto_mode') == 'thin'
        cflags += '-flto=thin'
    else
        cflags += '-flto'
    endif
endif

if get_option('b_staticpic')
    cflags += '-fPIC'
endif

cflags += get_option('cflags')

libffi = declare_dependency(
    dependencies: [
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--prefix=' + '@PREFIX@',
                '--bindir=' + '@PREFIX@' / '@BINDIR@',
                '--libdir=' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=' + '@PREFIX@' / 'lib',
                '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
                '--enable-static',
                '--disable-shared',
                'CFLAGS=' + ' '.join(cflags),
            ],
            cross_configure_options: [
                '--host=@HOST@',
            ],
            verbose: true,
        ).dependency('ffi').partial_dependency(
            compile_args: true,
            includes: true,
            sources: true,
        ),
    ],
    link_args: [
        meson.project_build_root() / 'dist' + get_option('prefix') / 'lib' / 'libffi.a',
    ],
)
