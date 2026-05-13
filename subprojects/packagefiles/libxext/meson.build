project('libxext', 'c', meson_version: '>=1.2.0')

libxext = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'Xext',
        include_directories: [
            'src',
            'include',
        ],
        sources: [
            'src/DPMS.c',
            'src/extutil.c',
            'src/globals.c',
            'src/MITMisc.c',
            'src/XAppgroup.c',
            'src/Xcup.c',
            'src/Xdbe.c',
            'src/XEVI.c',
            'src/Xge.c',
            'src/XLbx.c',
            'src/XMultibuf.c',
            'src/XSecurity.c',
            'src/XShape.c',
            'src/XShm.c',
            'src/XSync.c',
            'src/XTestExt1.c',
        ],
        dependencies: [
            dependency('x11', required: true),
            dependency('xau', required: true),
            dependency('xextproto', required: true),
            dependency('xproto', required: true),
        ],
    ),
    dependencies: [
        dependency('x11', required: true),
        dependency('xau', required: true),
        dependency('xextproto', required: true),
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xext', libxext)
