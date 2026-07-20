project('angle-compression-utils', 'cpp', meson_version: '>=1.4.0')

angle_compression_utils_portable = declare_dependency(
    include_directories: [
        'google',
    ],
    link_with: static_library(
        'angle_compression_utils_portable',
        include_directories: [
            'google',
        ],
        cpp_args: [
            '-DUSE_SYSTEM_ZLIB',
        ],
        sources: [
            'google/compression_utils_portable.cc',
        ],
        dependencies: [
            dependency('zlib', required: true),
        ],
        implicit_include_directories: false,
    ),
    dependencies: [
        dependency('zlib', required: true),
    ],
)
