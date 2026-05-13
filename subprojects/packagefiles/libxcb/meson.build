project('libxcb', 'c', meson_version: '>=1.10.0')

python = import('python').find_installation(required: true)
xcb_proto_root = subproject('xcb-proto').get_variable('xcb_proto_root')

xcb_proto_generated = []
foreach name : [
    'bigreq',
    'composite',
    'damage',
    'dbe',
    'dpms',
    'dri2',
    'dri3',
    'ge',
    'glx',
    'present',
    'randr',
    'record',
    'render',
    'res',
    'screensaver',
    'shape',
    'shm',
    'sync',
    'xc_misc',
    'xevie',
    'xf86dri',
    'xf86vidmode',
    'xfixes',
    'xinerama',
    'xinput',
    'xkb',
    'xprint',
    'xproto',
    'xselinux',
    'xtest',
    'xvmc',
    'xv',
]
    xcb_proto_generated += custom_target(
        name + '.h',
        output: [name + '.c', name + '.h'],
        command: [
            files('xcbgen.sh')[0],
            python,
            files('src/c_client.py')[0],
            '@PRIVATE_DIR@',
            '@OUTDIR@',
            xcb_proto_root,
            name,
        ],
        depend_files: [
            xcb_proto_root / 'src' / (name + '.xml'),
        ],
    )
    xcb_proto_generated += custom_target(
        'xcb ' + name + '.h',
        output: name + '.h',
        build_subdir: 'xcb',
        command: [
            'echo',
            '#include "../' + name + '.h"',
        ],
        capture: true,
    )
endforeach
foreach name : [
    'xcb',
    'xcbext',
    'xcbint',
    'xcb_windefs',
]
    xcb_proto_generated += custom_target(
        'xcb ' + name + '.h',
        output: name + '.h',
        build_subdir: 'xcb',
        command: [
            'echo',
            '#include "' + meson.project_source_root() / 'src' / name + '.h"',
        ],
        capture: true,
    )
endforeach

includes = [
    '.',
    'src',
]

libxcb = declare_dependency(
    include_directories: includes,
    link_with: library(
        'xcb',
        include_directories: includes,
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
