project('ruby', 'c', meson_version: '>=1.2.0')

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

# -std=gnu99 is needed with GCC 15 and higher (which default to gnu23), for Ruby versions that aren't valid C23.
# Ruby versions that are valid C23 are 3.2.9+, 3.3.9+, 3.4.5+, and 3.5.0+.
cflags += '-std=gnu99'

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

ruby_deps = [
    dependency('libcrypt', required: true),
    dependency('libffi', required: true),
    dependency('libyaml', required: true),
    dependency('z', required: true),
]

ruby = declare_dependency(
    dependencies: [
        ruby_deps,
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--srcdir=' + srcdir,
                '--prefix=' + meson.project_build_root() / 'dist' + '@PREFIX@',
                '--prefix=' + meson.project_build_root() / 'dist' + prefix,
                '--bindir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@BINDIR@',
                '--bindir=' + meson.project_build_root() / 'dist' + prefix / get_option('bindir'),
                '--libdir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=' + meson.project_build_root() / 'dist' + prefix / get_option('libdir'),
                '--includedir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@INCLUDEDIR@',
                '--includedir=' + meson.project_build_root() / 'dist' + prefix / get_option('includedir'),
                '--with-rubyhdrdir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@INCLUDEDIR@',
                '--with-rubyhdrdir=' + meson.project_build_root() / 'dist' + prefix / get_option('includedir'),
                '--with-rubyarchhdrdir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@INCLUDEDIR@',
                '--with-rubyarchhdrdir=' + meson.project_build_root() / 'dist' + prefix / get_option('includedir'),
                '--with-baseruby=' + find_program('ruby', required: true).full_path(),
                '--disable-shared',
                '--disable-install-doc',
                '--with-out-ext=openssl,readline,dbm,gdbm',
                '--with-static-linked-ext',
                '--disable-rubygems',
                '--without-gmp',
                'CFLAGS=' + ' '.join(cflags),
                'LDFLAGS=' + ' '.join(get_option('ldflags')),
            ],
            cross_configure_options: [
                '--host=@HOST@',
            ],
            env: {
                'CONFIG_SITE': ' ',
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
            compile_args: true,
            includes: true,
            sources: true,
        ),
    ],
    link_args: [
        meson.project_build_root() / 'build' / 'ext' / 'extinit.o',
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
        meson.project_build_root() / 'build' / 'ext' / 'pty' / 'pty.a',
        meson.project_build_root() / 'build' / 'ext' / 'racc' / 'cparse' / 'cparse.a',
        meson.project_build_root() / 'build' / 'ext' / 'rbconfig' / 'sizeof' / 'sizeof.a',
        meson.project_build_root() / 'build' / 'ext' / 'ripper' / 'ripper.a',
        meson.project_build_root() / 'build' / 'ext' / 'socket' / 'socket.a',
        meson.project_build_root() / 'build' / 'ext' / 'stringio' / 'stringio.a',
        meson.project_build_root() / 'build' / 'ext' / 'strscan' / 'strscan.a',
        meson.project_build_root() / 'build' / 'ext' / 'syslog' / 'syslog.a',
        meson.project_build_root() / 'build' / 'ext' / 'zlib' / 'zlib.a',
        meson.project_build_root() / 'build' / 'enc' / 'encinit.o',
        meson.project_build_root() / 'build' / 'enc' / 'libenc.a',
        meson.project_build_root() / 'build' / 'enc' / 'libtrans.a',
        meson.project_build_root() / 'build' / 'libruby-static.a',
    ],
)

meson.override_dependency('ruby-3.1', ruby, static: true)
