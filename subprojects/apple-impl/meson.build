project('apple-impl', 'objcpp', meson_version: '>=1.2.0')

apple_impl = declare_dependency(
    link_with: static_library(
        'apple-impl',
        sources: [
            'src/filesystem/filesystemImplApple.mm',
            'src/system/systemImplApple.mm',
        ],
        objcpp_args: get_option('objcppflags'),
        dependencies: [
            dependency('global-include-dirs', static: true),
        ],
    ),
)
