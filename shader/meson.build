
embedded_shaders = [
    'common.h',
    'transSimple.frag',
    'trans.frag',
    'hue.frag',
    'sprite.frag',
    'plane.frag',
    'gray.frag',
    'bitmapBlit.frag',
    'flatColor.frag',
    'simple.frag',
    'simpleColor.frag',
    'simpleAlpha.frag',
    'simpleAlphaUni.frag',
    'tilemap.frag',
    'flashMap.frag',
    'bicubic.frag',
    'lanczos3.frag',
    'minimal.vert',
    'simple.vert',
    'simpleColor.vert',
    'sprite.vert',
    'tilemap.vert',
    'tilemapvx.vert',
    'blur.frag',
    'blurH.vert',
    'blurV.vert',
    'simpleMatrix.vert',
    'kglInvert.frag',
    'kglCompressAlpha.frag',
    'kglSubtract.frag',
    'kglShadowH.frag',
    'kglShadowV.frag',
]

# xBRZ shader is GPLv3.
if get_option('enable-https') == true
    embedded_shaders += 'xbrz.frag'
endif

foreach shader : embedded_shaders
    global_dependencies += declare_dependency(sources: custom_target(
        '@0@.xxd'.format(shader),
        output: '@0@.xxd'.format(shader),
        command: [python, embedtool, files(shader)[0], '@OUTPUT@', 'mkxp_shader_@0@'.format(shader.replace('.', '_'))],
    ))
endforeach
