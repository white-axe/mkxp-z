project('libxcursor', 'c', meson_version: '>=1.2.0')

libxcursor = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'Xcursor',
        include_directories: [
            'src',
            'include',
            'include/X11/Xcursor',
        ],
        sources: [
            'src/cursor.c',
            'src/display.c',
            'src/file.c',
            'src/library.c',
            'src/xlib.c',
        ],
        c_args: [
            '-DHAVE_XFIXES',
        ],
        dependencies: [
            dependency('fixesproto', required: true),
            dependency('x11', required: true),
            dependency('xfixes', required: true),
            dependency('xrender', required: true),
        ],
    ),
    dependencies: [
        dependency('fixesproto', required: true),
        dependency('x11', required: true),
    ],
)

meson.override_dependency('xcursor', libxcursor)
