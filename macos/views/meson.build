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
                    files('settingsmenu.xib'),
                ],
            ),
            '@OUTPUT@',
            'mkxp_macos_settingsmenu_nib',
        ],
    ),
    custom_target(
        'gear.svg.xxd',
        output: 'gear.svg.xxd',
        command: [
            embedtool,
            files('gear.svg'),
            '@OUTPUT@',
            'mkxp_macos_gear_svg',
        ],
    ),
    custom_target(
        'gobackward.svg.xxd',
        output: 'gobackward.svg.xxd',
        command: [
            embedtool,
            files('gobackward.svg'),
            '@OUTPUT@',
            'mkxp_macos_gobackward_svg',
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
