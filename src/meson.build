physfs = dependency('physfs', version: '>=2.1', static: build_static)
openal = dependency('openal', static: build_static, method: 'pkg-config')
theora = dependency('theora', static: build_static)
vorbisfile = dependency('vorbisfile', static: build_static)
vorbis = dependency('vorbis', static: build_static)
ogg = dependency('ogg', static: build_static)
sdl2 = dependency('SDL2', static: build_static)
sdl_sound = compilers['cpp'].find_library('SDL2_sound')
sdl2_ttf = dependency('SDL2_ttf', static: build_static)
freetype = dependency('freetype2', static: build_static)
pixman = dependency('pixman-1', static: build_static)
png = dependency('libpng', static: build_static)
zlib = dependency('zlib')
uchardet = dependency('uchardet', static: build_static)

# As no pkg-config file is generated for static sdl2_image, and pkg-config is
# the default option for meson detecting dependencies, pkg-config will fail to
# find sdl2_image.pc in the build's lib/pkgconfig folder and instead pull it
# from the locally installed packages if it exists.
# To work around this, we first check to see if cmake can find our sdl2_image
# sub project and use that, then check using pkg-config as normal if we are not
# building the sub project.
# It looks like upstream SDL_image fixed this for SDL3, so we can hopefully
# remove this workaround after eventually upgrading to SDL3.
sdl2_image = dependency('SDL2_image', modules: ['SDL2_image::SDL2_image-static', 'SDL2_image::brotlidec-static', 'SDL2_image::brotlicommon-static', 'SDL2_image::hwy', 'SDL2_image::jxl_dec-static'], static: build_static, method: 'cmake', required: false)
if sdl2_image.found() == false
    sdl2_image = dependency('SDL2_image', modules: ['SDL2_image::SDL2_image-static', 'SDL2_image::brotlidec-static', 'SDL2_image::brotlicommon-static', 'SDL2_image::hwy', 'SDL2_image::jxl_dec-static'], static: build_static)
endif

if host_system == 'windows'
    bz2 = dependency('bzip2', static: build_static)
    iconv = compilers['cpp'].find_library('iconv', static: build_static)
else
    bz2 = compilers['cpp'].find_library('bz2')
    # FIXME: Specifically asking for static doesn't work if iconv isn't
    # installed in the system prefix somewhere
    iconv = compilers['cpp'].find_library('iconv')
    global_dependencies += compilers['cpp'].find_library('charset')
endif

# If OpenSSL is present, you get HTTPS support
if get_option('enable-https') == true
    openssl = dependency('openssl', required: false, static: build_static)
    if openssl.found() == true
        global_dependencies += openssl
        global_args += '-DMKXPZ_SSL'
        if host_system == 'windows'
            global_link_args += '-lcrypt32'
        endif
    else
        warning('Could not locate OpenSSL. HTTPS will be disabled.')
    endif
endif

cmake_base_options = {
    'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
    'CMAKE_OSX_SYSROOT': '/nonexistent',
    'CMAKE_C_STANDARD': c_std.replace('c', '').replace('gnu', ''),
    'CMAKE_CXX_STANDARD': cpp_std.replace('c++', '').replace('gnu++', ''),
    'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_OBJC_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_OBJCXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_BUILD_TYPE': 'Custom',
    'CMAKE_RC_COMPILER': host_system in ['windows', 'cygwin'] ? run_command(quotetool_command, find_program('windres'), check: true).stdout().strip() : '',
    'CMAKE_EXE_LINKER_FLAGS': common.get_variable('c_linker_flag'),
}

meson_base_options = {
    'default_library': 'static',
    'c_std': c_std,
    'cpp_std': cpp_std,
}

if true
    have_angle = get_option('angle').allowed()

    if have_angle
        require_system_xxhash = get_option('xxhash_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        xxhash = dependency('libxxhash', allow_fallback: false, required: get_option('angle').auto() and require_system_xxhash.enabled() ? get_option('angle') : require_system_xxhash)
        if not xxhash.found()
            if not get_option('xxhash_system').enabled()
                subproject('xxhash', default_options: meson_base_options)
            else
                have_angle = false
            endif
        endif
    endif

    spirv_headers_source_dir = subproject('spirv-headers', default_options: meson_base_options).get_variable('spirv_headers_source_dir')

    if have_angle
        have_angle_vulkan = get_option('angle_vulkan').allowed()
        require_system_spirv_tools = get_option('spirv_tools_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        spirv_tools = dependency('SPIRV-Tools', allow_fallback: false, required: get_option('angle').auto() and require_system_spirv_tools.enabled() ? get_option('angle') : require_system_spirv_tools)
        if not spirv_tools.found()
            if not require_system_spirv_tools.enabled()
                spirv_tools_options = cmake.subproject_options()
                spirv_tools_options.add_cmake_defines(cmake_base_options + {
                    'CMAKE_CXX_STANDARD': 17,
                    'CMAKE_C_FLAGS': run_command(quotetool_args, '-I' + spirv_headers_source_dir / 'include', check: true).stdout().strip(),
                    'CMAKE_CXX_FLAGS': run_command(quotetool_args, '-I' + spirv_headers_source_dir / 'include', check: true).stdout().strip(),
                    'SPIRV-Headers_SOURCE_DIR': spirv_headers_source_dir,
                    'BUILD_SHARED_LIBS': false,
                    'BUILD_TESTING': false,
                    'SKIP_SPIRV_TOOLS_INSTALL': true,
                    'SPIRV-TOOLS_USE_MIMALLOC': false,
                })
                meson.override_dependency('SPIRV-Tools', cmake.subproject('spirv-tools', options: spirv_tools_options).dependency('SPIRV-Tools-static'), static: true)
            else
                if get_option('angle_vulkan').enabled()
                    error('SPIRV-Tools is required for ANGLE Vulkan backend')
                endif
                have_angle = false
                have_angle_vulkan = false
            endif
        endif
    endif

    if have_angle and have_angle_vulkan
        require_system_volk = get_option('volk_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        volk = dependency('volk', allow_fallback: false, required: get_option('angle').auto() and require_system_spirv_tools.enabled() ? get_option('angle') : require_system_spirv_tools)
        if not volk.found()
            if not require_system_volk.enabled()
                global_dependencies += subproject('volk', default_options: meson_base_options).get_variable('volk')
            else
                if get_option('angle_vulkan').enabled()
                    error('volk is required for ANGLE Vulkan backend')
                endif
                have_angle = false
                have_angle_vulkan = false
            endif
        else
            global_dependencies += volk
        endif
    endif

    if have_angle
        if get_option('angle_direct3d9').disabled()
            angle_direct3d9 = 'disabled'
        elif get_option('angle_direct3d9').enabled()
            angle_direct3d9 = 'enabled'
        else
            angle_direct3d9 = 'auto'
        endif
        if get_option('angle_direct3d11').disabled()
            angle_direct3d11 = 'disabled'
        elif get_option('angle_direct3d11').enabled()
            angle_direct3d11 = 'enabled'
        else
            angle_direct3d11 = 'auto'
        endif
        if get_option('angle_metal').disabled()
            angle_metal = 'disabled'
        elif get_option('angle_metal').enabled()
            angle_metal = 'enabled'
        else
            angle_metal = 'auto'
        endif
        if get_option('angle_null').disabled()
            angle_null = 'disabled'
        elif get_option('angle_null').enabled()
            angle_null = 'enabled'
        else
            angle_null = 'auto'
        endif
        if get_option('angle_vulkan').disabled()
            angle_vulkan = 'disabled'
        elif get_option('angle_vulkan').enabled()
            angle_vulkan = 'enabled'
        else
            angle_vulkan = 'auto'
        endif
        angle = subproject('angle', default_options: meson_base_options + {
            'direct3d9': angle_direct3d9,
            'direct3d11': angle_direct3d11,
            'metal': angle_metal,
            'null': angle_null,
            'opengl': 'disabled',
            'vulkan': have_angle_vulkan ? angle_vulkan : 'disabled',
        })
        have_angle_direct3d9 = angle.get_variable('have_direct3d9')
        have_angle_direct3d11 = angle.get_variable('have_direct3d11')
        have_angle_metal = angle.get_variable('have_metal')
        have_angle_null = angle.get_variable('have_null')
        have_angle_vulkan = angle.get_variable('have_vulkan')
        global_args += '-DMKXPZ_HAVE_ANGLE'
        if have_angle_direct3d9
            global_args += '-DMKXPZ_HAVE_ANGLE_DIRECT3D9'
        endif
        if have_angle_direct3d11
            global_args += '-DMKXPZ_HAVE_ANGLE_DIRECT3D11'
        endif
        if have_angle_metal
            global_args += '-DMKXPZ_HAVE_ANGLE_METAL'
        endif
        if have_angle_null
            global_args += '-DMKXPZ_HAVE_ANGLE_NULL'
        endif
        if have_angle_vulkan
            gplv3_features += 'ANGLE Vulkan backend'
            global_args += '-DMKXPZ_HAVE_ANGLE_VULKAN'
        endif
    endif
endif

# Windows needs to be treated like a special needs child here
explicit_libs = ''
if host_system == 'windows'
    # Newer versions of Ruby will refuse to link without these
    explicit_libs += 'libmsvcrt;libgcc;libmingwex;libgmp;'
endif
if build_static == true
    if host_system == 'windows'
        # '-static-libgcc', '-static-libstdc++' are here to avoid needing to ship a separate libgcc_s_seh-1.dll on Windows; it still works without those flags if you have the dll.
        global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', '-Wl,-Bdynamic']
    else
        global_link_args += ['-static-libgcc', '-static-libstdc++']
    endif
    global_args += '-DAL_LIBTYPE_STATIC'
endif

foreach l : explicit_libs.split(';')
        if l != ''
            global_link_args += '-l:' + l + '.a'
        endif
endforeach

alcdev_struct = 'ALCdevice_struct'
if openal.type_name() == 'pkgconfig'
    if openal.version().version_compare('>=1.20.1')
        alcdev_struct = 'ALCdevice'
    endif
endif

global_args += '-DMKXPZ_ALCDEVICE=' + alcdev_struct


global_include_dirs += include_directories('.',
    'audio',
    'crypto',
    'display', 'display/gl', 'display/libnsgif', 'display/libnsgif/utils',
    'etc',
    'filesystem', 'filesystem/ghc',
    'input',
    'net',
    'system',
    'util', 'util/sigslot', 'util/sigslot/adapter'
)

global_dependencies += [openal, zlib, bz2, sdl2, sdl_sound, pixman, physfs, theora, vorbisfile, vorbis, ogg, sdl2_ttf, freetype, sdl2_image, png, iconv, uchardet, angle.get_variable('angle_dep')]
if host_system == 'windows'
    global_dependencies += compilers['cpp'].find_library('wsock32')
endif

if get_option('shared_fluid') == true
    fluidsynth = dependency('fluidsynth', static: build_static)
    add_project_arguments('-DSHARED_FLUID', language: 'cpp')
    global_dependencies += fluidsynth
    if host_system == 'windows'
        global_dependencies += compilers['cpp'].find_library('dsound')
    endif
endif

if get_option('cjk_fallback_font') == true
    add_project_arguments('-DMKXPZ_CJK_FONT', language: 'cpp')
endif

main_source = files(
    'main.cpp',
    'config.cpp',
    'eventthread.cpp',
    'settingsmenu.cpp',
    'sharedstate.cpp',
    
    'audio/alstream.cpp',
    'audio/audio.cpp',
    'audio/audiostream.cpp',
    'audio/fluid-fun.cpp',
    'audio/midisource.cpp',
    'audio/sdlsoundsource.cpp',
    'audio/soundemitter.cpp',
    'audio/vorbissource.cpp',
    'theoraplay/theoraplay.c',

    'crypto/rgssad.cpp',

    'display/autotiles.cpp',
    'display/autotilesvx.cpp',
    'display/bitmap.cpp',
    'display/font.cpp',
    'display/graphics.cpp',
    'display/plane.cpp',
    'display/sprite.cpp',
    'display/tilemap.cpp',
    'display/tilemapvx.cpp',
    'display/viewport.cpp',
    'display/window.cpp',
    'display/windowvx.cpp',

    'display/libnsgif/libnsgif.c',
    'display/libnsgif/lzw.c',

    'display/gl/gl-debug.cpp',
    'display/gl/gl-fun.cpp',
    'display/gl/gl-meta.cpp',
    'display/gl/glstate.cpp',
    'display/gl/scene.cpp',
    'display/gl/shader.cpp',
    'display/gl/texpool.cpp',
    'display/gl/tileatlas.cpp',
    'display/gl/tileatlasvx.cpp',
    'display/gl/tilequad.cpp',
    'display/gl/vertex.cpp',

    'util/iniconfig.cpp',
    'util/win-consoleutils.cpp',
    
    'etc/etc.cpp',
    'etc/table.cpp',

    'filesystem/filesystem.cpp',
    'filesystem/filesystemImpl.cpp',
    
    'input/input.cpp',
    'input/keybindings.cpp',

    'net/LUrlParser.cpp',
    'net/net.cpp',

    'system/systemImpl.cpp'
)

global_sources += main_source
