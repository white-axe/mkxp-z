embedded_assets = [
    'icon.png',
    'gamecontrollerdb.txt'
]

if get_option('cjk_fallback_font') == true
    embedded_assets += 'wqymicrohei.ttf'
else
    embedded_assets += 'liberation.ttf'
endif

embedded_assets_f = files(embedded_assets)

embedded_assets_sources = []

count = 0
foreach file : embedded_assets_f
    embedded_assets_sources += custom_target(embedded_assets[count],
    input: file,
    output: '@0@.xxd'.format(embedded_assets[count]),
    command: [
        embedtool,
        '@INPUT@',
        '@OUTPUT@',
        'mkxp_assets_@0@'.format(embedded_assets[count].replace('.', '_')),
    ],
    )
    count += 1
endforeach

global_dependencies += declare_dependency(sources: embedded_assets_sources)
