project('libxrandr', 'c', meson_version: '>=1.2.0')

libxrandr = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'Xrandr',
        include_directories: [
            'src',
            'include',
            'include/X11/extensions',
        ],
        sources: [
            'src/Xrandr.c',
            'src/Xrandrint.h',
            'src/XrrConfig.c',
            'src/XrrCrtc.c',
            'src/XrrMode.c',
            'src/XrrMonitor.c',
            'src/XrrOutput.c',
            'src/XrrProperty.c',
            'src/XrrProvider.c',
            'src/XrrProviderProperty.c',
            'src/XrrScreen.c',
        ],
        dependencies: [
            dependency('randrproto', required: true),
            dependency('renderproto', required: true),
            dependency('x11', required: true),
            dependency('xext', required: true),
            dependency('xextproto', required: true),
            dependency('xrender', required: true),
        ],
    ),
    dependencies: [
        dependency('randrproto', required: true),
        dependency('xrender', required: true),
    ],
)

meson.override_dependency('xrandr', libxrandr)
