global_dependencies += declare_dependency(sources: [
    custom_target(
        'settingsmenu.nib.xxd',
        output: 'settingsmenu.nib.xxd',
        command: [
            embedtool,
            custom_target(
                'settingsmenu.nib',
                output: 'settingsmenu.nib',
                command: [
                    find_program('ibtool'),
                    '--compile', '@OUTPUT@',
                    '--minimum-deployment-target', '10.13',
                    files('settingsmenu.xib'),
                ],
            ),
            '@OUTPUT@',
            'mkxp_macos_settingsmenu_nib',
        ],
    ),
    custom_target(
        'gear.png.xxd',
        output: 'gear.png.xxd',
        command: [
            embedtool,
            files('gear/gear_Normal@2x_5.png'),
            '@OUTPUT@',
            'mkxp_macos_gear_png',
        ],
    ),
    custom_target(
        'gobackward.png.xxd',
        output: 'gobackward.png.xxd',
        command: [
            embedtool,
            files('gobackward/gobackward_Normal@2x_5.png'),
            '@OUTPUT@',
            'mkxp_macos_gobackward_png',
        ],
    ),
])

global_sources += files(
    'SettingsMenuController.mm',
    'TouchBar.mm',
)

global_include_dirs += include_directories(
    '.',
)
