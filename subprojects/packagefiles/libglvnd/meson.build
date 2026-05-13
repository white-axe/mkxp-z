project('libglvnd', 'c', meson_version: '>=1.2.0')

libglvnd = declare_dependency(include_directories: 'include')

meson.override_dependency('egl', libglvnd, static: true)
meson.override_dependency('gl', libglvnd, static: true)
meson.override_dependency('glesv2', libglvnd, static: true)
meson.override_dependency('glesv3', libglvnd, static: true)
meson.override_dependency('glx', libglvnd, static: true)
meson.override_dependency('opengl', libglvnd, static: true)
