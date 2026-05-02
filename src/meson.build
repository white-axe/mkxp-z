cmake = import('cmake')

cmake_base_options = {
    'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
    'CMAKE_OSX_SYSROOT': '/nonexistent',
    'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
}

meson_base_options = {
    'default_library': 'static',
    'b_staticpic': use_pic,
}

physfs_options = cmake.subproject_options()
physfs_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'PHYSFS_BUILD_STATIC': true,
    'PHYSFS_BUILD_SHARED': false,
    'PHYSFS_BUILD_TEST': false,
    'PHYSFS_BUILD_DOCS': false,
})
global_dependencies += cmake.subproject('physfs', options: physfs_options).dependency('physfs-static')

pipewire = dependency('libpipewire-0.3', version: '>=0.3.23', method: 'pkg-config', required: false)
openal_options = cmake.subproject_options()
openal_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'LIBTYPE': 'STATIC',
    'ALSOFT_DLOPEN': false,
    'ALSOFT_UTILS': false,
    'ALSOFT_NO_CONFIG_UTIL': true,
    'ALSOFT_EXAMPLES': false,
    'ALSOFT_UPDATE_BUILD_VERSION': false,
    'ALSOFT_EMBED_HRTF_DATA': false,
    'ALSOFT_RTKIT': false,
    'ALSOFT_BACKEND_PIPEWIRE': pipewire.found(),
    'ALSOFT_BACKEND_PULSEAUDIO': true,
    'ALSOFT_BACKEND_ALSA': true,
    'ALSOFT_BACKEND_OSS': true,
    'ALSOFT_BACKEND_SOLARIS': true,
    'ALSOFT_BACKEND_SNDIO': true,
    'ALSOFT_BACKEND_WINMM': true,
    'ALSOFT_BACKEND_DSOUND': true,
    'ALSOFT_BACKEND_WASAPI': true,
    'ALSOFT_BACKEND_OTHERIO': true,
    'ALSOFT_BACKEND_JACK': true,
    'ALSOFT_BACKEND_COREAUDIO': true,
    'ALSOFT_BACKEND_OBOE': true,
    'ALSOFT_BACKEND_OPENSL': true,
    'ALSOFT_BACKEND_PORTAUDIO': true,
    'ALSOFT_BACKEND_SDL3': false,
    'ALSOFT_BACKEND_SDL2': false,
    'ALSOFT_BACKEND_WAVE': true,
})
global_dependencies += pipewire
global_dependencies += cmake.subproject('openal-soft', options: openal_options).dependency('OpenAL')
global_dependencies += cmake.subproject('openal-soft', options: openal_options).dependency('alsoft.fmt')

ogg_options = cmake.subproject_options()
ogg_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
    'BUILD_FRAMEWORK': false,
})
global_dependencies += cmake.subproject('ogg', options: ogg_options).dependency('ogg')
ogg_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'ogg' / 'include', '-I' + meson.global_build_root() / 'subprojects' / 'ogg' / '__CMake_build' / 'include']

vorbis_options = cmake.subproject_options()
vorbis_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(ogg_includes + global_args),
    'CMAKE_CXX_FLAGS': ' '.join(ogg_includes + global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
    'BUILD_FRAMEWORK': false,
})
global_dependencies += cmake.subproject('vorbis', options: vorbis_options).dependency('vorbis')
global_dependencies += cmake.subproject('vorbis', options: vorbis_options).dependency('vorbisfile')

sdl2_options = cmake.subproject_options()
sdl2_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'LIBTYPE': 'STATIC',
    'BUILD_SHARED_LIBS': false,
    'SDL_DBUS': true,
    'SDL_DISKAUDIO': false,
    'SDL_DUMMYAUDIO': false,
    'SDL_DIRECTFB': false,
    'SDL_DIRECTFB_SHARED': false,
    'SDL_DUMMYVIDEO': false,
    'SDL_IBUS': true,
    'SDL_SYSTEM_ICONV': false,
    'SDL_OPENGL': true,
    'SDL_OPENGLES': true,
    'SDL_OSS': false,
    'SDL_ALSA': false,
    'SDL_ALSA_SHARED': false,
    'SDL_JACK': false,
    'SDL_JACK_SHARED': false,
    'SDL_ESD': false,
    'SDL_ESD_SHARED': false,
    'SDL_PIPEWIRE': false,
    'SDL_PIPEWIRE_SHARED': false,
    'SDL_PULSEAUDIO': false,
    'SDL_PULSEAUDIO_SHARED': false,
    'SDL_ARTS': false,
    'SDL_ARTS_SHARED': false,
    'SDL_NAS': false,
    'SDL_NAS_SHARED': false,
    'SDL_SNDIO': false,
    'SDL_SNDIO_SHARED': false,
    'SDL_FUSIONSOUND': false,
    'SDL_FUSIONSOUND_SHARED': false,
    'SDL_LIBSAMPLERATE': false,
    'SDL_LIBSAMPLERATE_SHARED': false,
})
global_dependencies += cmake.subproject('sdl2', options: sdl2_options).dependency('SDL2-static')
sdl2_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'sdl2' / 'include', '-I' + meson.global_build_root() / 'subprojects' / 'sdl2' / '__CMake_build' / 'include-config-release' / 'SDL2']

sdl2_sound_options = cmake.subproject_options()
sdl2_sound_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(sdl2_includes + global_args),
    'CMAKE_CXX_FLAGS': ' '.join(sdl2_includes + global_args + global_cpp_args),
    'SDLSOUND_BUILD_SHARED': false,
    'SDLSOUND_BUILD_TEST': false,
})
global_dependencies += cmake.subproject('sdl2_sound', options: sdl2_sound_options).dependency('SDL2_sound-static')

freetype_options = meson_base_options + {
    'cflags': global_args,
    'cppflags': global_args + global_cpp_args,
    'brotli': 'disabled',
    'bzip2': 'disabled',
    'harfbuzz': 'disabled',
    'mmap': 'disabled',
    'png': 'disabled',
    'tests': 'disabled',
    'zlib': 'disabled',
}
global_dependencies += subproject('freetype', default_options: freetype_options).get_variable('freetype_dep')
freetype_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'freetype' / 'include']

sdl2_ttf_options = cmake.subproject_options()
sdl2_ttf_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(sdl2_includes + freetype_includes + global_args),
    'CMAKE_CXX_FLAGS': ' '.join(sdl2_includes + freetype_includes + global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
})
global_dependencies += cmake.subproject('sdl2_ttf', options: sdl2_ttf_options).dependency('SDL2_ttf')

pixman_options = meson_base_options + {
    'cflags': global_args,
    'cppflags': global_args + global_cpp_args,
    'libpng': 'disabled',
    'tls': 'disabled',
}
global_dependencies += subproject('pixman', default_options: pixman_options).get_variable('idep_pixman')

zlib_options = cmake.subproject_options()
zlib_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
    'ZLIB_BUILD_EXAMPLES': false,
})
global_dependencies += cmake.subproject('zlib', options: zlib_options).dependency('z')
zlib_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'zlib', '-I' + meson.global_build_root() / 'subprojects' / 'zlib' / '__CMake_build']
zlib_libs = ['-L' + meson.global_build_root() / 'subprojects' / 'zlib']

uchardet_options = cmake.subproject_options()
uchardet_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'BUILD_BINARY': false,
    'BUILD_SHARED_LIBS': false,
})
global_dependencies += cmake.subproject('uchardet', options: uchardet_options).dependency('libuchardet')

theora_options = meson_base_options + {
    'cflags': global_args,
    'cppflags': global_args + global_cpp_args,
}
global_dependencies += subproject('theora', default_options: theora_options).get_variable('theoradec')

libiconv_options = meson_base_options + {
    'cflags': global_args,
    'cppflags': global_args + global_cpp_args,
}
global_dependencies += subproject('libiconv', default_options: libiconv_options).get_variable('libiconv')

sdl2_image_options = cmake.subproject_options()
sdl2_image_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(sdl2_includes + global_args),
    'CMAKE_CXX_FLAGS': ' '.join(sdl2_includes + global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
    'SDL2IMAGE_JPG_SAVE': true,
    'SDL2IMAGE_PNG_SAVE': true,
    'SDL2IMAGE_PNG_SHARED': false,
    'SDL2IMAGE_JPG_SHARED': false,
    'SDL2IMAGE_JXL': true,
    'SDL2IMAGE_JXL_SHARED': false,
    'SDL2IMAGE_VENDORED': true,
    'SDL2IMAGE_SAMPLES': false,
    'JPEGXL_ENABLE_DEVTOOLS': false,
    'JPEGXL_ENABLE_EXAMPLES': false,
    'JPEGXL_ENABLE_FUZZERS': false,
    'JPEGXL_ENABLE_MANPAGES': false,
    'JPEGXL_ENABLE_TOOLS': false,
})
global_dependencies += cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('SDL2_image')
global_dependencies += cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('brotlicommon-static')
global_dependencies += cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('brotlidec-static')
global_dependencies += cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('hwy')
global_dependencies += cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('jxl_dec-static')

fluidsynth_options = cmake.subproject_options()
fluidsynth_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(global_args),
    'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
    'enable-alsa': false,
    'enable-aufile': false,
    'enable-dbus': false,
    'enable-ipv6 ': false,
    'enable-jack': false,
    'enable-ladspa': false,
    'enable-libinstpatch': false,
    'enable-libsndfile': false,
    'enable-midishare': false,
    'enable-opensles': false,
    'enable-oboe': false,
    'enable-network': false,
    'enable-oss': false,
    'enable-dsound': false,
    'enable-waveout': false,
    'enable-winmidi': false,
    'enable-sdl3': false,
    'enable-pulseaudio': false,
    'enable-pipewire': false,
    'enable-readline': false,
    'enable-threads': false,
    'enable-openmp': false,
    'enable-unicode': false,
    'enable-native-dls': false,
    'enable-systemd': false,
    'enable-coreaudio': false,
    'enable-coremidi': false,
    'enable-framework': false,
    'enable-dart': false,
    'enable-kai': false,
    'osal': 'embedded',
})
global_dependencies += cmake.subproject('fluidsynth', options: fluidsynth_options).dependency('libfluidsynth')

# If OpenSSL is present, you get HTTPS support
if get_option('enable-https') == true
    openssl_options = meson_base_options
    global_dependencies += subproject('openssl', default_options: openssl_options).get_variable('openssl_dep')
    global_args += '-DMKXPZ_SSL'
endif

libyaml_options = cmake.subproject_options()
libyaml_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': ' '.join(sdl2_includes + global_args),
    'CMAKE_CXX_FLAGS': ' '.join(sdl2_includes + global_args + global_cpp_args),
    'BUILD_SHARED_LIBS': false,
})
global_dependencies += cmake.subproject('libyaml', options: libyaml_options).dependency('yaml')
libyaml_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'libyaml' / 'include', '-I' + meson.global_build_root() / 'subprojects' / 'libyaml' / '__CMake_build' / 'include']
libyaml_libs = ['-L' + meson.global_build_root() / 'subprojects' / 'libyaml']

libxcrypt_options = meson_base_options
global_dependencies += subproject('libxcrypt', default_options: libxcrypt_options).get_variable('libxcrypt')
libxcrypt_includes = ['-I' + meson.global_build_root() / 'subprojects' / 'libxcrypt-4.5.2' / 'dist' + get_option('prefix') / get_option('includedir')]
libxcrypt_libs = ['-L' + meson.global_build_root() / 'subprojects' / 'libxcrypt-4.5.2' / 'dist' + get_option('prefix') / get_option('libdir')]

ruby_options = meson_base_options + {
    'cflags': zlib_includes + libyaml_includes + libxcrypt_includes,
    'cppflags': zlib_includes + libyaml_includes + libxcrypt_includes,
    'ldflags': zlib_libs + libyaml_libs + libxcrypt_libs,
}
global_dependencies += subproject('ruby', default_options: ruby_options).get_variable('ruby')

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

global_args += '-DMKXPZ_ALCDEVICE=ALCdevice'


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

global_cpp_args += '-DSHARED_FLUID'

if get_option('cjk_fallback_font') == true
    global_cpp_args += '-DMKXPZ_CJK_FONT'
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
