project('libffi', 'c', 'cpp', meson_version: '>=1.3.0')

find_program('cmp', required: true)
find_program('diff', required: true)

cflags = subproject('tools').get_variable('base_cflags')
cppflags = subproject('tools').get_variable('base_cppflags')

cflags += get_option('c_args')
cppflags += get_option('cpp_args')

ldflags = subproject('tools').get_variable('base_ldflags')

ldflags += get_option('c_link_args')

srcdir = meson.project_source_root()
if build_machine.system() in ['windows', 'cygwin']
    srcdir = run_command(find_program('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

libffi_includes = [
    '-I' + meson.project_build_root() / 'dist' / 'include',
]

libffi_libs = [
    '-L' + meson.project_build_root() / 'dist' / 'lib',
]

libffi = declare_dependency(
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
                'CXX=' + run_command(subproject('tools').get_variable('quotetool_command'), meson.get_compiler('cpp').cmd_array(), check: true).stdout().strip(),
                'AR=' + run_command(subproject('tools').get_variable('quotetool_command'), find_program('ar'), check: true).stdout().strip(),
                'RANLIB=' + run_command(subproject('tools').get_variable('quotetool_command'), find_program('ranlib'), check: true).stdout().strip(),
                'CFLAGS=' + run_command(subproject('tools').get_variable('quotetool_args'), cflags, check: true).stdout().strip(),
                'CXXFLAGS=' + run_command(subproject('tools').get_variable('quotetool_args'), cppflags, check: true).stdout().strip(),
                'LDFLAGS=' + run_command(subproject('tools').get_variable('quotetool_args'), ldflags, check: true).stdout().strip(),
            ],
            cross_configure_options: [
                '--host=@HOST@',
                '--host=' + subproject('tools').get_variable('host_triplet'),
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
        libffi_includes,
    ],
    link_args: [
        meson.project_build_root() / 'dist' / 'lib' / 'libffi.a',
    ],
)

meson.override_dependency('libffi', libffi, static: true)
