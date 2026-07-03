project('libudev', 'c', meson_version: '>=1.3.0')

libudev = declare_dependency(
    include_directories: [
        'src/libudev',
    ],
)

meson.override_dependency('libudev', libudev, static: true)
