project('ruby-3.1', 'c', 'cpp', version: '3.1.3', meson_version: '>=1.2.0')

find_program('cmp', required: true)

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

# -std=gnu99 is needed with GCC 15 and higher (which default to gnu23), for Ruby versions that aren't valid C23.
# Ruby versions that are valid C23 are 3.2.9+, 3.3.9+, 3.4.5+, and 3.5.0+.
cflags += '-std=gnu99'

cflags += get_option('c_args')

ldflags = []

linker_id = meson.get_compiler('c').get_linker_id()
if linker_id == 'ld64' or linker_id == 'ld64.lld'
    ldflags += '-Wl,-w'
endif

if get_option('b_lto')
    if get_option('b_lto_mode') == 'thin' and meson.get_compiler('c').has_argument('-flto=thin')
        ldflags += '-flto=thin'
    elif meson.get_compiler('c').has_argument('-flto=auto')
        ldflags += '-flto=auto'
    else
        ldflags += '-flto'
    endif
endif

ldflags += get_option('c_link_args')

srcdir = meson.project_source_root()
if build_machine.system() == 'windows' or build_machine.system() == 'cygwin'
    srcdir = run_command(find_program('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

destdir = meson.project_build_root() / 'dist'
if build_machine.system() == 'windows' or build_machine.system() == 'cygwin'
    destdir = run_command(find_program('cygpath'), '-m', destdir, check: true).stdout().strip()
endif

baseruby = find_program('ruby', required: true).full_path()
if build_machine.system() == 'windows' or build_machine.system() == 'cygwin'
    baseruby = run_command(find_program('cygpath'), '-u', baseruby, check: true).stdout().strip()
endif
baseruby = run_command(subproject('tools').get_variable('quotetool'), baseruby, check: true).stdout().strip()

bison = find_program('bison', required: true).full_path()
if build_machine.system() == 'windows' or build_machine.system() == 'cygwin'
    bison = run_command(find_program('cygpath'), '-u', bison, check: true).stdout().strip()
endif
bison = run_command(subproject('tools').get_variable('quotetool'), bison, check: true).stdout().strip()

ruby_deps = [
    dependency('libffi', required: true),
    dependency('yaml-0.1', required: true),
    dependency('zlib', required: true),
]

ruby = declare_dependency(
    dependencies: [
        ruby_deps,
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
                '--with-rubyhdrdir=/include',
                '--with-rubyarchhdrdir=/include',
                '--with-destdir=' + destdir,
                '--with-baseruby=' + baseruby,
                '--with-soname=ruby',
                '--disable-shared',
                '--disable-install-doc',
                '--with-out-ext=openssl,readline,dbm,gdbm',
                '--with-static-linked-ext',
                '--disable-rubygems',
                '--without-gmp',
                'CC=' + run_command(subproject('tools').get_variable('quotetool'), meson.get_compiler('c').cmd_array(), check: true).stdout().strip(),
                'CXX=' + run_command(subproject('tools').get_variable('quotetool'), meson.get_compiler('cpp').cmd_array(), check: true).stdout().strip(),
                'CFLAGS=' + run_command(subproject('tools').get_variable('quotetool'), cflags, check: true).stdout().strip() + ' ' + get_option('cflags'),
                'CXXFLAGS=' + run_command(subproject('tools').get_variable('quotetool'), cflags, check: true).stdout().strip() + ' ' + get_option('cflags'),
                'LDFLAGS=' + run_command(subproject('tools').get_variable('quotetool'), ldflags, check: true).stdout().strip() + ' ' + get_option('ldflags'),
            ],
            cross_configure_options: [
                '--host=@HOST@',
            ],
            env: {
                'CONFIG_SITE': ' ',
                'YACC': bison,
                'ac_cv_func_crypt': 'no',
                'ac_cv_func_crypt_r': 'no',
                'ac_cv_header_crypt_h': 'no',
                'ac_cv_lib_crypt_crypt': 'no',
                'ac_cv_lib_z_uncompress': 'yes',
                'rb_cv_broken_crypt': 'no',
            },
            depends: [
                # Force Meson to fully build all of our vendored dependencies that are required by Ruby before building Ruby itself
                executable(
                    'deps',
                    sources: [
                        'deps-stub.c',
                    ],
                    dependencies: [
                        ruby_deps,
                    ],
                ),
            ],
            verbose: true,
        ).dependency('ruby-static').partial_dependency(
            sources: true,
        ),
        host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
            ? [
                meson.get_compiler('c').find_library('shell32', required: true),
                meson.get_compiler('c').find_library('ws2_32', required: true),
                meson.get_compiler('c').find_library('iphlpapi', required: true),
                meson.get_compiler('c').find_library('imagehlp', required: true),
                meson.get_compiler('c').find_library('shlwapi', required: true),
                meson.get_compiler('c').find_library('bcrypt', required: true),
            ]
            : [],
    ],
    compile_args: [
        '-I' + meson.project_build_root() / 'dist' / 'include',
    ],
    link_args: [
        (host_machine.system() == 'windows' or host_machine.system() == 'cygwin') and not meson.get_compiler('c').has_header_symbol('math.h', 'lgamma_r')
            ? [
                meson.project_build_root() / 'build' / 'lgamma_r.o',
            ]
            : [],
        meson.project_build_root() / 'build' / 'ext' / 'extinit.a',
        meson.project_build_root() / 'build' / 'ext' / 'bigdecimal' / 'bigdecimal.a',
        meson.project_build_root() / 'build' / 'ext' / 'cgi' / 'escape' / 'escape.a',
        meson.project_build_root() / 'build' / 'ext' / 'continuation' / 'continuation.a',
        meson.project_build_root() / 'build' / 'ext' / 'coverage' / 'coverage.a',
        meson.project_build_root() / 'build' / 'ext' / 'date' / 'date_core.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'digest.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'bubblebabble' / 'bubblebabble.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'md5' / 'md5.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'rmd160' / 'rmd160.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'sha1' / 'sha1.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'sha2' / 'sha2.a',
        meson.project_build_root() / 'build' / 'ext' / 'etc' / 'etc.a',
        meson.project_build_root() / 'build' / 'ext' / 'fcntl' / 'fcntl.a',
        meson.project_build_root() / 'build' / 'ext' / 'fiddle' / 'fiddle.a',
        meson.project_build_root() / 'build' / 'ext' / 'io' / 'console' / 'console.a',
        meson.project_build_root() / 'build' / 'ext' / 'io' / 'nonblock' / 'nonblock.a',
        meson.project_build_root() / 'build' / 'ext' / 'io' / 'wait' / 'wait.a',
        meson.project_build_root() / 'build' / 'ext' / 'json' / 'generator' / 'generator.a',
        meson.project_build_root() / 'build' / 'ext' / 'json' / 'parser' / 'parser.a',
        meson.project_build_root() / 'build' / 'ext' / 'monitor' / 'monitor.a',
        meson.project_build_root() / 'build' / 'ext' / 'nkf' / 'nkf.a',
        meson.project_build_root() / 'build' / 'ext' / 'objspace' / 'objspace.a',
        meson.project_build_root() / 'build' / 'ext' / 'pathname' / 'pathname.a',
        meson.project_build_root() / 'build' / 'ext' / 'psych' / 'psych.a',
        meson.project_build_root() / 'build' / 'ext' / 'racc' / 'cparse' / 'cparse.a',
        meson.project_build_root() / 'build' / 'ext' / 'rbconfig' / 'sizeof' / 'sizeof.a',
        meson.project_build_root() / 'build' / 'ext' / 'ripper' / 'ripper.a',
        meson.project_build_root() / 'build' / 'ext' / 'socket' / 'socket.a',
        meson.project_build_root() / 'build' / 'ext' / 'stringio' / 'stringio.a',
        meson.project_build_root() / 'build' / 'ext' / 'strscan' / 'strscan.a',
        meson.project_build_root() / 'build' / 'ext' / 'zlib' / 'zlib.a',
        host_machine.system() == 'windows' or host_machine.system() == 'cygwin'
            ? [
                meson.project_build_root() / 'build' / 'ext' / 'win32' / 'resolv' / 'resolv.a',
                meson.project_build_root() / 'build' / 'ext' / 'win32ole' / 'win32ole.a',
            ]
            : [
                meson.project_build_root() / 'build' / 'ext' / 'pty' / 'pty.a',
                meson.project_build_root() / 'build' / 'ext' / 'syslog' / 'syslog.a',
            ],
        meson.project_build_root() / 'build' / 'enc' / 'encinit.a',
        meson.project_build_root() / 'build' / 'enc' / 'libenc.a',
        meson.project_build_root() / 'build' / 'enc' / 'libtrans.a',
        meson.project_build_root() / 'build' / 'libruby-static.a',
    ],
)

meson.override_dependency('ruby-3.1', ruby, static: true)
