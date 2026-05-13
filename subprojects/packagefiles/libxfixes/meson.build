project('libxfixes', 'c', meson_version: '>=1.10.0')

libxfixes = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'Xfixes',
        include_directories: [
            'src',
            'include',
            'include/X11/extensions',
        ],
        sources: [
            'src/Cursor.c',
            'src/Disconnect.c',
            'src/Region.c',
            'src/SaveSet.c',
            'src/Selection.c',
            'src/Xfixes.c',
        ],
        dependencies: [
            dependency('fixesproto', required: true),
            dependency('x11', required: true),
            dependency('xextproto', required: true),
            dependency('xproto', required: true),
        ],
    ),
    dependencies: [
        dependency('x11', required: true),
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xfixes', libxfixes)
