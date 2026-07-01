project('libinotify', 'c', meson_version: '>=1.2.0')

cflags = subproject('tools').get_variable('base_cflags')

cflags += get_option('c_args')

srcdir = meson.project_source_root()
if build_machine.system() in ['windows', 'cygwin']
    srcdir = run_command(find_program('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

libinotify = declare_dependency(
    dependencies: [
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--srcdir=' + srcdir,
                '--prefix=' + '@PREFIX@',
                '--prefix=/',
                meson.version().version_compare('>=1.7.0') ? '--bindir=' + '@PREFIX@' / '@BINDIR@' : [],
                '--bindir=/bin',
                '--libdir=' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=/lib',
                '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
                '--includedir=/include',
                '--enable-static',
                '--disable-shared',
                'CC=' + run_command(subproject('tools').get_variable('quotetool_command'), meson.get_compiler('c').cmd_array(), check: true).stdout().strip(),
                'AR=' + run_command(subproject('tools').get_variable('quotetool_command'), find_program('ar'), check: true).stdout().strip(),
                'RANLIB=' + run_command(subproject('tools').get_variable('quotetool_command'), find_program('ranlib'), check: true).stdout().strip(),
                'CFLAGS=' + run_command(subproject('tools').get_variable('quotetool_args'), cflags, check: true).stdout().strip(),
            ],
            cross_configure_options: [
                '--host=@HOST@',
                '--host=' + subproject('tools').get_variable('host_triplet'),
            ],
            env: {
                'CONFIG_SITE': ' ',
            },
            verbose: true,
        ).dependency('inotify').partial_dependency(
            sources: true,
        ),
    ],
    compile_args: [
        '-I' + meson.project_build_root() / 'dist' / 'include',
    ],
    link_args: [
        meson.project_build_root() / 'dist' / 'lib' / 'libinotify.a',
    ],
)

meson.override_dependency('libinotify', libinotify, static: true)
