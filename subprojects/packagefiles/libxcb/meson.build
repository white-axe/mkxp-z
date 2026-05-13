project('libxcb', 'c', meson_version: '>=1.2.0')

python = import('python').find_installation(required: true)
xcb_proto_root = subproject('xcb-proto').get_variable('xcb_proto_root')
xcb_proto_sources = subproject('xcb-proto').get_variable('xcb_proto_sources')
xcb_proto_generated = []

subdir('include')

libxcb = declare_dependency(
    include_directories: [
        'include',
    ],
    link_with: library(
        'xcb',
        include_directories: [
            'src',
            'include',
        ],
        sources: xcb_proto_generated + [
            'src/xcb_auth.c',
            'src/xcb_conn.c',
            'src/xcb_ext.c',
            'src/xcb_in.c',
            'src/xcb_list.c',
            'src/xcb_out.c',
            'src/xcb_util.c',
            'src/xcb_xid.c',
        ],
        dependencies: [
            declare_dependency(sources: xcb_proto_generated),
            dependency('xau', required: true),
        ],
        c_args: [
            '-DIOV_MAX=16',
            '-DXCB_QUEUE_BUFFER_SIZE=16384',
        ],
    ),
)

meson.override_dependency('xcb', libxcb)
meson.override_dependency('x11-xcb', libxcb)
