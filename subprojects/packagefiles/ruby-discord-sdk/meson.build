project('ruby-discord-sdk', 'c', meson_version: '>=1.4.0')

custom_target(
    'ruby-discord-sdk.stamp',
    output: 'ruby-discord-sdk.stamp',
    command: [
        subproject('common').get_variable('python'),
        files('build.py'),
        host_machine.system(),
        meson.project_source_root(),
        meson.project_build_root(),
        subproject('ruby-' + get_option('mri_version')).get_variable('miniruby'),
        find_program('make'),
        host_machine.system() == 'darwin' ? find_program('lipo') : '',
        '@OUTPUT@',
    ],
    build_by_default: true,
)

meson.add_install_script(
    subproject('common').get_variable('python'),
    files('install.py'),
    meson.project_build_root(),
    host_machine.system(),
    '',
    install_tag: get_option('install_tag'),
)

if host_machine.system() == 'darwin'
    meson.add_install_script(
        subproject('common').get_variable('python'),
        files('install.py'),
        meson.project_build_root(),
        host_machine.system(),
        get_option('exe_name') + '.app',
        install_tag: get_option('install_tag') + '-bundle',
    )
endif
