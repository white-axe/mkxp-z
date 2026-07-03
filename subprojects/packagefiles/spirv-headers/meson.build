project('spirv-headers', 'c', meson_version: '>=1.3.0')

spirv_headers = declare_dependency(include_directories: 'include')

meson.override_dependency('SPIRV-Headers', spirv_headers, static: true)
