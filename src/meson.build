cmake = import('cmake')

is_nix = host_system != 'windows' and host_system != 'cygwin' and host_system != 'darwin' and host_system != 'android'

null_dep = dependency('', required: false)
foreach dep_name : [
    host_system != 'linux' ? 'alsa' : '',
    'audioio',
    'avahi-client',
    not is_nix ? 'dbus-1' : '',
    'epoll-shim',
    'fftw3f',
    not is_nix ? 'gio-2.0' : '',
    'girepository-2.0',
    not is_nix ? 'glib-2.0' : '',
    not is_nix ? 'gmodule-2.0' : '',
    not is_nix ? 'gobject-2.0' : '',
    'gobject-introspection-1.0',
    'gthread-2.0',
    'gtk+-3.0',
    not is_nix ? 'ibus-1.0' : '',
    'intl',
    not is_nix ? 'jack' : '',
    'libapparmor',
    'libasyncns',
    'libcanberra',
    'libcrypt',
    not is_nix ? 'libdecor-0' : '',
    'libdrm',
    'libffado',
    'libinotify',
    'libpcre2-8',
    not is_nix ? 'libpulse' : '',
    'libpulse-simple',
    'libselinux',
    'libsystemd',
    not is_nix ? 'libudev' : '',
    'libusb-1.0',
    'libxcrypt',
    'mount',
    'ncursesw',
    'oboe',
    host_system != 'android' ? 'opensl' : '',
    not get_option('enable-https') ? 'openssl' : '',
    'opus',
    'oss',
    'portaudio',
    'roc',
    'sdl3',
    'snapd-glib',
    'snapd-glib-2',
    not is_nix ? 'sndfile' : '',
    host_system != 'dragonfly' and host_system != 'freebsd' and host_system != 'netbsd' and host_system != 'openbsd' ? 'sndio' : '',
    'systemd',
    not is_nix ? 'wayland-client' : '',
    not is_nix ? 'wayland-cursor' : '',
    not is_nix ? 'wayland-egl' : '',
    not is_nix ? 'wayland-egl-backend' : '',
    not is_nix ? 'wayland-protocols' : '',
    not is_nix ? 'wayland-scanner' : '',
    not is_nix ? 'wayland-server' : '',
    'vulkan',
    not is_nix ? 'x11' : '',
    not is_nix ? 'x11-xcb' : '',
    not is_nix ? 'xau' : '',
    not is_nix ? 'xcb' : '',
    not is_nix ? 'xcursor' : '',
    not is_nix ? 'xext' : '',
    not is_nix ? 'xfixes' : '',
    not is_nix ? 'xi' : '',
    not is_nix ? 'xkbcommon' : '',
    not is_nix ? 'xkbcommon-x11' : '',
    not is_nix ? 'xrandr' : '',
    not is_nix ? 'xrender' : '',
    'xscrnsaver',
    'xtrans',
]
    if dep_name != ''
        meson.override_dependency(dep_name, null_dep, static: true)
    endif
endforeach

cmake_base_options = {
    'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
    'CMAKE_OSX_SYSROOT': '/nonexistent',
    'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_OBJC_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_OBJCXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
    'CMAKE_BUILD_TYPE': 'Custom',
}

meson_base_options = {
    'default_library': 'static',
    'b_staticpic': use_pic,
}

subproject('libglvnd', default_options: meson_base_options)

libiconv_options = meson_base_options
libiconv = subproject('libiconv', default_options: libiconv_options).get_variable('libiconv')
global_dependencies += libiconv

zlib_options = cmake.subproject_options()
zlib_options.add_cmake_defines(cmake_base_options + {
    'BUILD_SHARED_LIBS': false,
    'ZLIB_BUILD_EXAMPLES': false,
})
zlib = cmake.subproject('zlib', options: zlib_options).dependency('z')
zlib_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'zlib', '-I' + meson.global_build_root() / 'subprojects' / 'zlib' / '__CMake_build']
zlib_libs = ['-L' + meson.global_build_root() / 'subprojects' / 'zlib']
meson.override_dependency('zlib', zlib, static: true)
global_dependencies += zlib

libffi_options = meson_base_options
libffi = subproject('libffi', default_options: libffi_options).get_variable('libffi')
libffi_includes = ['-I' + meson.global_build_root() / 'subprojects' / 'libffi-3.5.2' / 'dist' / 'include']
libffi_libs = ['-L' + meson.global_build_root() / 'subprojects' / 'libffi-3.5.2' / 'dist' / 'lib']

# If OpenSSL is present, you get HTTPS support
if get_option('enable-https')
    openssl_options = meson_base_options
    openssl = subproject('openssl', default_options: openssl_options).get_variable('openssl_dep')
    meson.override_dependency('openssl', openssl, static: true)
    global_dependencies += openssl
    global_args += '-DMKXPZ_SSL'
endif

physfs_options = cmake.subproject_options()
physfs_options.add_cmake_defines(cmake_base_options + {
    'PHYSFS_BUILD_STATIC': true,
    'PHYSFS_BUILD_SHARED': false,
    'PHYSFS_BUILD_TEST': false,
    'PHYSFS_BUILD_DOCS': false,
})
physfs = declare_dependency(dependencies: [
    cmake.subproject('physfs', options: physfs_options).dependency('physfs-static'),
    host_system == 'darwin'
        ? dependency(
            'appleframeworks',
            modules: [
                'Foundation',
                'IOKit',
            ],
            required: true,
        )
        : [],
])
meson.override_dependency('physfs', physfs, static: true)
global_dependencies += physfs

fmt_options = cmake.subproject_options()
fmt_options.add_cmake_defines(cmake_base_options + {
    'LIBTYPE': 'STATIC',
    'FMT_DOC': false,
    'FMT_TEST': false,
    'FMT_FUZZ': false,
    'FMT_CUDA_TEST': false,
    'FMT_MODULE': false,
    'FMT_WERROR': false,
})
fmt = cmake.subproject('fmt', options: fmt_options).dependency('fmt')
meson.override_dependency('fmt', fmt, static: true)

if is_nix
    subproject('expat', default_options: meson_base_options)
    subproject('libudev', default_options: meson_base_options)
    subproject('dbus', default_options: meson_base_options)
    subproject('glib', default_options: meson_base_options + {
        'bsymbolic_functions': false,
        'dtrace': 'disabled',
        'glib_assert': false,
        'glib_checks': false,
        'glib_debug': 'disabled',
        'introspection': 'disabled',
        'libelf': 'disabled',
        'sysprof': 'disabled',
        'systemtap': 'disabled',
        'tests': false,
        'xattr': false,
    })
    subproject('ibus', default_options: meson_base_options)
    subproject('xorgproto', default_options: meson_base_options)
    subproject('libxau', default_options: meson_base_options)
    subproject('libxcb', default_options: meson_base_options)
    subproject('libx11', default_options: meson_base_options)
    subproject('libxfixes', default_options: meson_base_options)
    subproject('libxext', default_options: meson_base_options)
    subproject('libxi', default_options: meson_base_options)
    subproject('libxrender', default_options: meson_base_options)
    subproject('libxrandr', default_options: meson_base_options)
    subproject('libxcursor', default_options: meson_base_options)
    subproject('wayland', default_options: meson_base_options)
    subproject('libxkbcommon', default_options: meson_base_options)
    subproject('libdecor', default_options: meson_base_options)
endif

subproject('xxhash', default_options: meson_base_options)
spirv_headers = subproject('spirv-headers', default_options: meson_base_options).get_variable('spirv_headers')
spirv_tools_options = cmake.subproject_options()
spirv_tools_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': run_command(python, quotetool, host_system, 'false', '-I' + meson.project_source_root() / 'subprojects' / 'spirv-headers' / 'include', check: true).stdout().strip(),
    'CMAKE_CXX_FLAGS': run_command(python, quotetool, host_system, 'false', '-I' + meson.project_source_root() / 'subprojects' / 'spirv-headers' / 'include', check: true).stdout().strip(),
    'SPIRV-Headers_SOURCE_DIR': meson.project_source_root() / 'subprojects' / 'spirv-headers',
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
    'SPIRV-TOOLS_USE_MIMALLOC': false,
})
meson.override_dependency('SPIRV-Tools', cmake.subproject('spirv-tools', options: spirv_tools_options).dependency('SPIRV-Tools-static'), static: true)
subproject('volk', default_options: meson_base_options)
subproject('angle', default_options: meson_base_options)

sdl2_options = meson_base_options + {
    'test': false,
    'run_test': false,
    'use_atomic': 'enabled',
    'use_audio': 'disabled',
    'use_cpuinfo': 'enabled',
    'use_events': 'enabled',
    'use_file': 'enabled',
    'use_filesystem': 'enabled',
    'use_joystick': 'enabled',
    'use_haptic': 'disabled',
    'use_hidapi': 'disabled',
    'use_loadso': 'enabled',
    'use_locale': 'disabled',
    'use_render': 'enabled',
    'use_power': 'enabled',
    'use_sensor': 'disabled',
    'use_threads': 'enabled',
    'use_timers': 'enabled',
    'use_video': 'enabled',
    'use_video_x11': is_nix ? 'enabled' : 'disabled',
    'use_video_wayland': is_nix ? 'enabled' : 'disabled',
    'use_video_wayland_libdecor': is_nix ? 'enabled' : 'disabled',
    'use_video_opengl': 'enabled',
    'use_video_openglesv2': 'enabled',
    'use_video_vulkan': 'disabled',
    'use_video_offscreen': 'disabled',
    'prefer_dlopen': is_nix,
    'system_iconv': 'enabled',
    'angle': 'auto',
}
sdl2main = declare_dependency(
    dependencies: subproject('sdl2', default_options: sdl2_options).get_variable('sdl2main_dep'),
    link_args: host_system == 'windows' or host_system == 'cygwin'
        ? ['-lmingw32']
        : [],
)
sdl2 = subproject('sdl2', default_options: sdl2_options).get_variable('sdl2_dep')
sdl2_includes = ['-I' + meson.global_build_root() / 'subprojects' / 'SDL2-2.28.1' / 'include', '-I' + meson.project_source_root() / 'subprojects' / 'SDL2-2.28.1' / 'include']
global_dependencies += [sdl2main, sdl2]

if host_system == 'linux'
    alsa_lib_options = meson_base_options
    alsa_lib = subproject('alsa-lib', default_options: alsa_lib_options).get_variable('alsa_lib')
endif

if is_nix
    libsndfile_options = cmake.subproject_options()
    libsndfile_options.add_cmake_defines(cmake_base_options + {
        'BUILD_SHARED_LIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_EXAMPLES': false,
        'BUILD_REGTEST': false,
        'BUILD_TESTING': false,
        'ENABLE_CPACK': false,
        'ENABLE_PACKAGE_CONFIG': false,
        'INSTALL_PKGCONFIG_MODULE': false,
        'ENABLE_EXTERNAL_LIBS': false,
        'ENABLE_MPEG': false,
        'ENABLE_EXPERIMENTAL': false,
    })
    libsndfile = cmake.subproject('libsndfile', options: libsndfile_options).dependency('sndfile')
    meson.override_dependency('sndfile', libsndfile, static: true)

    jack1_options = meson_base_options
    jack1 = subproject('jack1', default_options: jack1_options).get_variable('jack1')

    pulseaudio_options = meson_base_options + {
        'daemon': false,
        'client': true,
        'doxygen': false,
        'gcov': false,
        'man': false,
        'tests': false,
        'udev': 'disabled',
        'x11': 'disabled',
        'oss-output': 'disabled',
        'dbus': 'disabled',
        'glib': 'disabled',
    }
    pulseaudio = subproject('pulseaudio', default_options: pulseaudio_options).get_variable('libpulse_dep')
    meson.override_dependency('libpulse', pulseaudio, static: true)

    pipewire_options = meson_base_options + {
        'docs': 'disabled',
        'man': 'disabled',
        'examples': 'disabled',
        'tests': 'disabled',
        'gstreamer': 'disabled',
        'gstreamer-device-provider': 'disabled',
        'spa-plugins': 'disabled',
        'pipewire-jack': 'disabled',
        'pipewire-v4l2': 'disabled',
        'ffmpeg': 'disabled',
        'dbus': 'disabled',
        'pw-cat-ffmpeg': 'disabled',
        'echo-cancel-webrtc': 'disabled',
        'session-managers': [],
        'flatpak': 'disabled',
        'readline': 'disabled',
        'gsettings': 'disabled',
        'udev': 'disabled',
        'x11': 'disabled',
        'x11-xfixes': 'disabled',
    }
    pipewire = subproject('pipewire', default_options: pipewire_options).get_variable('pipewire_dep')
endif

openal_options = meson_base_options + {
    'rtkit': 'disabled',
    'sdl2': 'disabled',
    'utils': false,
}
openal = subproject('openal-soft', default_options: openal_options).get_variable('openal_dep')
global_dependencies += openal

ogg_options = cmake.subproject_options()
ogg_options.add_cmake_defines(cmake_base_options + {
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
    'BUILD_FRAMEWORK': false,
})
ogg = cmake.subproject('ogg', options: ogg_options).dependency('ogg')
ogg_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'ogg' / 'include', '-I' + meson.global_build_root() / 'subprojects' / 'ogg' / '__CMake_build' / 'include']
meson.override_dependency('ogg', ogg, static: true)
global_dependencies += ogg

vorbis_options = cmake.subproject_options()
vorbis_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': run_command(python, quotetool, host_system, 'false', ogg_includes, check: true).stdout().strip(),
    'CMAKE_CXX_FLAGS': run_command(python, quotetool, host_system, 'false', ogg_includes, check: true).stdout().strip(),
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
    'BUILD_FRAMEWORK': false,
})
vorbis = declare_dependency(dependencies: [
    ogg,
    cmake.subproject('vorbis', options: vorbis_options).dependency('vorbis'),
])
vorbisfile = declare_dependency(dependencies: [
    vorbis,
    cmake.subproject('vorbis', options: vorbis_options).dependency('vorbisfile'),
])
meson.override_dependency('vorbis', vorbis, static: true)
meson.override_dependency('vorbisfile', vorbisfile, static: true)
global_dependencies += [vorbis, vorbisfile]

sdl2_sound_options = cmake.subproject_options()
sdl2_sound_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': run_command(python, quotetool, host_system, 'false', sdl2_includes, check: true).stdout().strip(),
    'CMAKE_CXX_FLAGS': run_command(python, quotetool, host_system, 'false', sdl2_includes, check: true).stdout().strip(),
    'SDLSOUND_BUILD_SHARED': false,
    'SDLSOUND_BUILD_TEST': false,
})
sdl2_sound = declare_dependency(dependencies: [
    sdl2,
    cmake.subproject('sdl2_sound', options: sdl2_sound_options).dependency('SDL2_sound-static'),
])
meson.override_dependency('SDL2_sound', sdl2_sound, static: true)
global_dependencies += sdl2_sound

freetype_options = meson_base_options + {
    'brotli': 'disabled',
    'bzip2': 'disabled',
    'harfbuzz': 'disabled',
    'mmap': 'disabled',
    'png': 'disabled',
    'tests': 'disabled',
    'zlib': 'disabled',
}
freetype = subproject('freetype', default_options: freetype_options).get_variable('freetype_dep')
freetype_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'freetype' / 'include']
global_dependencies += freetype

sdl2_ttf_options = cmake.subproject_options()
sdl2_ttf_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': run_command(python, quotetool, host_system, 'false', freetype_includes + sdl2_includes, check: true).stdout().strip(),
    'CMAKE_CXX_FLAGS': run_command(python, quotetool, host_system, 'false', freetype_includes + sdl2_includes, check: true).stdout().strip(),
    'BUILD_SHARED_LIBS': false,
    'BUILD_TESTING': false,
})
sdl2_ttf = declare_dependency(dependencies: [
    freetype,
    sdl2,
    cmake.subproject('sdl2_ttf', options: sdl2_ttf_options).dependency('SDL2_ttf'),
])
meson.override_dependency('SDL2_ttf', sdl2_ttf, static: true)
global_dependencies += sdl2_ttf

pixman_options = meson_base_options + {
    'libpng': 'disabled',
    'tls': 'disabled',
} + (
    host_system == 'darwin'
        ? {
            'a64-neon': 'disabled',
        }
        : {}
)
pixman = subproject('pixman', default_options: pixman_options).get_variable('idep_pixman')
global_dependencies += pixman

uchardet_options = cmake.subproject_options()
uchardet_options.add_cmake_defines(cmake_base_options + {
    'BUILD_BINARY': false,
    'BUILD_SHARED_LIBS': false,
})
uchardet = cmake.subproject('uchardet', options: uchardet_options).dependency('libuchardet')
meson.override_dependency('uchardet', uchardet, static: true)
global_dependencies += uchardet

theora_options = meson_base_options
theora = subproject('theora', default_options: theora_options).get_variable('theoradec')
meson.override_dependency('theora', theora, static: true)
global_dependencies += theora

sdl2_image_options = cmake.subproject_options()
sdl2_image_options.add_cmake_defines(cmake_base_options + {
    'CMAKE_C_FLAGS': run_command(python, quotetool, host_system, 'false', sdl2_includes, check: true).stdout().strip(),
    'CMAKE_CXX_FLAGS': run_command(python, quotetool, host_system, 'false', sdl2_includes, check: true).stdout().strip(),
    'BUILD_SHARED_LIBS': false,
    'SDL2IMAGE_JPG_SAVE': true,
    'SDL2IMAGE_PNG_SAVE': true,
    'SDL2IMAGE_PNG_SHARED': false,
    'SDL2IMAGE_JPG_SHARED': false,
    'SDL2IMAGE_JXL': true,
    'SDL2IMAGE_JXL_SHARED': false,
    'SDL2IMAGE_VENDORED': true,
    'SDL2IMAGE_SAMPLES': false,
    'SDL2IMAGE_BACKEND_WIC': false,
    'SDL2IMAGE_BACKEND_IMAGEIO': false,
    'JPEGXL_ENABLE_DEVTOOLS': false,
    'JPEGXL_ENABLE_EXAMPLES': false,
    'JPEGXL_ENABLE_FUZZERS': false,
    'JPEGXL_ENABLE_MANPAGES': false,
    'JPEGXL_ENABLE_TOOLS': false,
    'JPEGXL_STATIC': true,
})
sdl2_image = declare_dependency(dependencies: [
    sdl2,
    cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('SDL2_image'),
    cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('brotlicommon-static'),
    cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('brotlidec-static'),
    cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('hwy'),
    cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('jxl_dec-static'),
])
meson.override_dependency('SDL2_image', sdl2_image, static: true)
global_dependencies += sdl2_image

fluidsynth_options = cmake.subproject_options()
fluidsynth_options.add_cmake_defines(cmake_base_options + {
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
fluidsynth = cmake.subproject('fluidsynth', options: fluidsynth_options).dependency('libfluidsynth')
meson.override_dependency('fluidsynth', fluidsynth, static: true)
global_dependencies += fluidsynth

libyaml_options = cmake.subproject_options()
libyaml_options.add_cmake_defines(cmake_base_options + {
    'BUILD_SHARED_LIBS': false,
})
libyaml = cmake.subproject('libyaml', options: libyaml_options).dependency('yaml')
libyaml_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'libyaml' / 'include', '-I' + meson.global_build_root() / 'subprojects' / 'libyaml' / '__CMake_build' / 'include']
libyaml_libs = ['-L' + meson.global_build_root() / 'subprojects' / 'libyaml']
meson.override_dependency('libyaml', libyaml, static: true)

ruby_options = meson_base_options + {
    'cflags': zlib_includes + libyaml_includes + libffi_includes,
    'cppflags': zlib_includes + libyaml_includes + libffi_includes,
    'ldflags': zlib_libs + libyaml_libs + libffi_libs,
}
ruby = subproject('ruby-3.1', default_options: ruby_options).get_variable('ruby')
global_dependencies += ruby

if build_static == true
    if host_system == 'windows'
        # '-static-libgcc', '-static-libstdc++' are here to avoid needing to ship a separate libgcc_s_seh-1.dll on Windows; it still works without those flags if you have the dll.
        global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', '-Wl,-Bdynamic']
    endif
endif

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
    host_system == 'darwin' ? 'filesystem/filesystemImplApple.mm' : 'filesystem/filesystemImpl.cpp',

    'input/input.cpp',
    'input/keybindings.cpp',

    'net/LUrlParser.cpp',
    'net/net.cpp',

    host_system == 'darwin' ? 'system/systemImplApple.mm' : 'system/systemImpl.cpp',
)

global_sources += main_source
