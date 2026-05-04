project('libxcrypt', 'c', meson_version: '>=1.2.0')

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

srcdir = meson.project_source_root()
if host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
    srcdir = import('fs').as_posix(srcdir)
    if not srcdir.startswith('/')
        srcdir = '/' + srcdir
    endif
    if srcdir.contains(':')
        srcdir_components_in = srcdir.split(':')
        srcdir_components_out = []
        i = 0
        foreach srcdir_component : srcdir_components_in
            if i == 0
                srcdir_components_out += srcdir_component.to_lower()
            else
                srcdir_components_out += srcdir_component
            endif
            i += 1
        endforeach
        srcdir = ''.join(srcdir_components_out)
    endif
endif

prefix = get_option('prefix')
if not import('fs').is_absolute(prefix)
    prefix = meson.global_source_root() / prefix
endif

libxcrypt = declare_dependency(
    dependencies: [
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--srcdir=' + srcdir,
                '--prefix=' + '@PREFIX@',
                '--prefix=' + prefix,
                '--bindir=' + '@PREFIX@' / '@BINDIR@',
                '--bindir=' + prefix / get_option('bindir'),
                '--libdir=' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=' + prefix / get_option('libdir'),
                '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
                '--includedir=' + prefix / get_option('includedir'),
                '--enable-static',
                '--disable-shared',
                '--disable-werror',
                'CFLAGS=' + ' '.join(cflags),
            ],
            cross_configure_options: [
                '--host=@HOST@',
            ],
            env: {
                'CONFIG_SITE': ' ',
            },
            verbose: true,
        ).dependency('crypt').partial_dependency(
            compile_args: true,
            includes: true,
            sources: true,
        ),
    ],
    link_args: [
        meson.project_build_root() / 'dist' + prefix / get_option('libdir') / 'libcrypt.a',
    ],
)
