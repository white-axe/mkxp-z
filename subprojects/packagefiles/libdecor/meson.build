project('libdecor', 'c', meson_version: '>=1.2.0')

libdecor = declare_dependency(
    include_directories: 'src',
    dependencies: [
        dependency('wayland-client', required: true),
    ],
)

meson.override_dependency('libdecor-0', libdecor, static: true)
