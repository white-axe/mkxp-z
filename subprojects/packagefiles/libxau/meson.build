project('libxau', 'c', meson_version: '>=1.2.0')

libxau = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'Xau',
        include_directories: [
            '.',
            'include',
        ],
        sources: [
            'AuDispose.c',
            'AuFileName.c',
            'AuGetAddr.c',
            'AuGetBest.c',
            'AuLock.c',
            'AuRead.c',
            'AuUnlock.c',
            'AuWrite.c',
        ],
        c_args: [
            '-DXTHREADS',
            '-DXUSE_MTSAFE_API',
        ],
        dependencies: [
            dependency('xproto', required: true),
        ],
    ),
    dependencies: [
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xau', libxau)
