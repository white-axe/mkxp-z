project('alsa-lib', 'c', meson_version: '>=1.2.0')

cflags = []

if get_option('optimization') == '0'
    cflags += '-O0'
elif get_option('optimization') == 'g'
    cflags += '-Og'
elif get_option('optimization') == '1'
    cflags += '-O1'
elif get_option('optimization') == '2'
    cflags += '-O2'
elif get_option('optimization') == '3'
    cflags += '-O3'
elif get_option('optimization') == 's'
    cflags += '-Os'
endif

if get_option('debug')
    cflags += '-g'
endif

if get_option('b_lto')
    if get_option('b_lto_mode') == 'thin' and meson.get_compiler('c').has_argument('-flto=thin')
        cflags += '-flto=thin'
    elif meson.get_compiler('c').has_argument('-flto=auto')
        cflags += '-flto=auto'
    else
        cflags += '-flto'
    endif
endif

if get_option('b_staticpic')
    cflags += '-fPIC'
endif

cflags += get_option('c_args')

srcdir = meson.project_source_root()
if build_machine.system() == 'windows' or build_machine.system() == 'cygwin'
    srcdir = run_command(find_program('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

alsa_lib = declare_dependency(
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
                'CC=' + run_command(subproject('tools').get_variable('quotetool'), build_machine.system(), 'true', meson.get_compiler('c').cmd_array(), check: true).stdout().strip(),
                'AR=' + run_command(subproject('tools').get_variable('quotetool'), build_machine.system(), 'true', find_program('ar'), check: true).stdout().strip(),
                'RANLIB=' + run_command(subproject('tools').get_variable('quotetool'), build_machine.system(), 'true', find_program('ranlib'), check: true).stdout().strip(),
                'CFLAGS=' + run_command(subproject('tools').get_variable('quotetool'), build_machine.system(), 'false', cflags, check: true).stdout().strip(),
            ],
            cross_configure_options: [
                '--host=@HOST@',
                '--host=' + subproject('tools').get_variable('host_triplet'),
            ],
            env: {
                'CONFIG_SITE': ' ',
            },
            verbose: true,
        ).dependency('asound').partial_dependency(
            sources: true,
        ),
    ],
    compile_args: [
        '-I' + meson.project_build_root() / 'dist' / 'include',
    ],
    link_args: [
        meson.project_build_root() / 'dist' / 'lib' / 'libasound.a',
    ],
)

meson.override_dependency('alsa', alsa_lib, static: true)
