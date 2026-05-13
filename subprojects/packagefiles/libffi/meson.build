project('libffi', 'c', meson_version: '>=1.2.0')

find_program('cmp', required: true)
find_program('diff', required: true)

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
    srcdir = srcdir.replace('\\', '/')
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

libffi = declare_dependency(
    dependencies: [
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--srcdir=' + srcdir,
                '--prefix=' + '@PREFIX@',
                '--prefix=/',
                '--bindir=' + '@PREFIX@' / '@BINDIR@',
                '--bindir=/bin',
                '--libdir=' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=/lib',
                '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
                '--includedir=/include',
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
            sources: true,
        ),
    ],
    compile_args: [
        '-I' + meson.project_build_root() / 'dist' / 'include',
    ],
    link_args: [
        meson.project_build_root() / 'dist' / 'lib' / 'libffi.a',
    ],
)

meson.override_dependency('libffi', libffi, static: true)
