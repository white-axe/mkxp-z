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

libxcrypt = import('unstable-external_project').add_project(
    'configure',
    configure_options: [
        '--prefix=' + '@PREFIX@',
        '--bindir=' + '@PREFIX@' / '@BINDIR@',
        '--libdir=' + '@PREFIX@' / '@LIBDIR@',
        '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
        '--enable-static',
        '--disable-shared',
        'CFLAGS=' + ' '.join(cflags),
    ],
    cross_configure_options: [
        '--host=@HOST@',
    ],
    verbose: true,
).dependency('crypt')
