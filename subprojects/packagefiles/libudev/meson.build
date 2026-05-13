project('libudev', 'c', meson_version: '>=1.10.0')

libudev = declare_dependency(
    include_directories: [
        'src/libudev',
    ],
)

meson.override_dependency('libudev', libudev)
