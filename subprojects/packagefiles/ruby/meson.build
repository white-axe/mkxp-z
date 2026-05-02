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

ruby = import('unstable-external_project').add_project(
    'configure',
    configure_options: [
        '--prefix=' + meson.project_build_root() / 'dist' + '@PREFIX@',
        '--bindir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@BINDIR@',
        '--libdir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@LIBDIR@',
        '--includedir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@INCLUDEDIR@',
        '--with-rubyhdrdir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@INCLUDEDIR@',
        '--with-rubyarchhdrdir=' + meson.project_build_root() / 'dist' + '@PREFIX@' / '@INCLUDEDIR@',
        '--with-baseruby=' + find_program('ruby', required: true).full_path(),
        '--enable-shared',
        '--disable-install-doc',
        '--with-out-ext=openssl,readline,dbm,gdbm',
        '--with-static-linked-ext',
        '--disable-rubygems',
        '--without-gmp',
        'CFLAGS=' + ' '.join(cflags),
        'LDFLAGS=' + ' '.join(get_option('ldflags')),
    ],
    env: {
        # Allows Ruby's build system to find our vendored zlib and libyaml
        'PKG_CONFIG_PATH': meson.global_build_root() / 'meson-uninstalled',
    },
    verbose: true,
).dependency('ruby')
