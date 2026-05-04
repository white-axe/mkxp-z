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

prefix = get_option('prefix')
if not import('fs').is_absolute(prefix)
    prefix = meson.global_source_root() / prefix
endif

libffi = declare_dependency(
    dependencies: [
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--prefix=' + '@PREFIX@',
                '--prefix=' + prefix,
                '--bindir=' + '@PREFIX@' / '@BINDIR@',
                '--bindir=' + prefix / get_option('bindir'),
                '--libdir=' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=' + prefix / 'lib',
                '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
                '--includedir=' + prefix / get_option('includedir'),
                '--enable-static',
                '--disable-shared',
                'CFLAGS=' + ' '.join(cflags),
            ],
            cross_configure_options: [
                '--host=@HOST@',
            ],
            env: {
                'CONFIG_SITE': ' ',
            },
            verbose: true,
        ).dependency('ffi').partial_dependency(
            compile_args: true,
            includes: true,
            sources: true,
        ),
    ],
    link_args: [
        meson.project_build_root() / 'dist' + prefix / 'lib' / 'libffi.a',
    ],
)
