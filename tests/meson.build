test_harness = [
    files('test-harness.py'),
    exe,
    meson.project_source_root(),
    meson.project_build_root(),
]

# Graphics tests
foreach test_name, test_options : {
    'benchmark-bitmap-dup': {'is_benchmark': true},
    'kgl2/blank': {'is_benchmark': false},
    'kgl2/clear': {'is_benchmark': false},
    'kgl2/clear-framebuffer': {'is_benchmark': false},
    'kgl2/clone': {'is_benchmark': false},
    'kgl2/compress-alpha': {'is_benchmark': false},
    'kgl2/invert': {'is_benchmark': false},
    'kgl2/light': {'is_benchmark': false},
    'kgl2/shadow': {'is_benchmark': false},
}
    if test_options['is_benchmark']
        benchmark(test_name + '+opengl+sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=true'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-sub-image-fix'])
        benchmark(test_name + '+opengl+no-sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=false'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-no-sub-image-fix'])
        if have_angle_vulkan
            benchmark(test_name + '+vulkan', python, args: [test_harness, test_name, 'renderer=3;subImageFix=false'], suite: ['graphics', 'graphics-vulkan'])
        endif
        if have_angle_metal
            benchmark(test_name + '+metal', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-metal'])
        endif
        if have_angle_direct3d11
            benchmark(test_name + '+direct3d11', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-direct3d11'])
        endif
    else
        test(test_name + '+opengl+sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=true'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-sub-image-fix'])
        test(test_name + '+opengl+no-sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=false'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-no-sub-image-fix'])
        if have_angle_vulkan
            test(test_name + '+vulkan', python, args: [test_harness, test_name, 'renderer=3;subImageFix=false'], suite: ['graphics', 'graphics-vulkan'])
        endif
        if have_angle_metal
            test(test_name + '+metal', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-metal'])
        endif
        if have_angle_direct3d11
            test(test_name + '+direct3d11', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-direct3d11'])
        endif
    endif
endforeach

# Misc. tests
foreach test_name : [
    'http',
]
    test(test_name, python, args: [test_harness, test_name, ''])
endforeach
