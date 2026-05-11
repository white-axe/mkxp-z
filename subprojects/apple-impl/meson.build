project('apple-impl', 'objcpp', meson_version: '>=1.2.0')

include_dirs = [
    'views',
]

apple_impl = declare_dependency(
    include_directories: include_dirs,
    link_with: static_library(
        'apple-impl',
        sources: [
            'src/filesystem/filesystemImplApple.mm',
            'src/system/systemImplApple.mm',
            'views/SettingsMenuController.mm',
            'views/TouchBar.mm',
        ],
        include_directories: include_dirs,
        objcpp_args: get_option('objcppflags'),
        dependencies: [
            dependency('mkxp-global-dependencies', static: true),
        ],
    ),
)
