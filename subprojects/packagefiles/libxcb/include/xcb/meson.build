foreach name : xcb_proto_sources
    xcb_proto_generated += custom_target(
        name + '.h',
        output: [name + '.c', name + '.h'],
        command: [
            files('../../xcbgen.sh')[0],
            python,
            files('../../src/c_client.py')[0],
            '@PRIVATE_DIR@',
            '@OUTDIR@',
            xcb_proto_root,
            name,
        ],
        depend_files: [
            xcb_proto_root / 'src' / (name + '.xml'),
        ],
    )
endforeach
