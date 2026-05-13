project('libxrender', 'c', meson_version: '>=1.2.0')

libxrender = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'Xrender',
        include_directories: [
            'src',
            'include',
            'include/X11/extensions',
        ],
        sources: [
            'src/AddTrap.c',
            'src/Color.c',
            'src/Composite.c',
            'src/Cursor.c',
            'src/FillRect.c',
            'src/FillRects.c',
            'src/Filter.c',
            'src/Glyph.c',
            'src/Picture.c',
            'src/Poly.c',
            'src/Trap.c',
            'src/Tri.c',
            'src/Xrender.c',
        ],
        dependencies: [
            dependency('renderproto', required: true),
            dependency('x11', required: true),
        ],
    ),
    dependencies: [
        dependency('renderproto', required: true),
        dependency('x11', required: true),
    ],
)

meson.override_dependency('xrender', libxrender)
