project('libxcb', 'c', meson_version: '>=1.3.0')

python = import('python').find_installation(required: true)
xcb_proto_root = subproject('xcb-proto').get_variable('xcb_proto_root')
xcb_proto_sources = subproject('xcb-proto').get_variable('xcb_proto_sources')
xcb_proto_generated = []

subdir('include')

libxcb = declare_dependency(
    include_directories: [
        'include',
    ],
    sources: xcb_proto_generated,
)

meson.override_dependency('xcb', libxcb, static: true)
meson.override_dependency('x11-xcb', libxcb, static: true)
