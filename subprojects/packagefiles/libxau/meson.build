project('libxau', 'c', meson_version: '>=1.2.0')

libxau = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xau', libxau)
