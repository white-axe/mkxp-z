project('mkxp-z', 'c', 'cpp', version: '2.4.2', meson_version: '>=1.2.0')
if host_machine.system() == 'darwin'
    add_languages('objcpp', native: false)
endif

gplv3_features = []

host_system = host_machine.system()
host_endian = host_machine.endian()

use_pic = true

compilers = {'cpp': meson.get_compiler('cpp')}

git_hash_file = vcs_tag(
    input: 'src/git-hash.h.in',
    output: 'git-hash.h',
    command: ['git', 'rev-parse', '--short=7', 'HEAD'],
    fallback: 'unknown',
)

global_sources = []
global_dependencies = [declare_dependency(sources: git_hash_file)]
global_include_dirs = []
global_args = []
global_cpp_args = []
global_objc_args = ['-fobjc-arc']
global_objcpp_args = []
global_link_args = []
global_std = ['c_std=c99', 'cpp_std=c++14']

python = subproject('tools').get_variable('embedtool')
embedtool = subproject('tools').get_variable('embedtool')
quotetool = subproject('tools').get_variable('quotetool')

strip = []
if get_option('strip')
    strip = find_program('strip', required: false)
    if not strip.found()
        strip = []
    endif
endif

sizeof = {'void*': compilers['cpp'].sizeof('void*'),
          'long': compilers['cpp'].sizeof('long')
         }
win64 = (sizeof['void*'] != sizeof['long'])

global_args += '-DMKXPZ_VERSION="@0@"'.format(meson.project_version())
global_args += '-DHAVE_NANOSLEEP'
# ====================
# Ext libs
# ====================

# STEAMWORKS

steamworks = false
steamworks_path = get_option('steamworks_path')
if steamworks_path != ''
    libname = 'steam_api'
    if host_system == 'linux'
        if sizeof['void*'] == 4
            bindir = 'linux32'
        else
            bindir = 'linux64'
        endif
    else
        if win64 == true
            bindir = 'win64'
            libname += '64'
        else
            bindir = ''
        endif
    endif

    steam_libpath = steamworks_path + '/redistributable_bin/' + bindir
    steamlib = compilers['cpp'].find_library(libname, required: false, dirs: [steam_libpath])

    if steamlib.found() == true
        global_include_dirs += include_directories('steamshim')
        global_args += '-DMKXPZ_STEAM'
        global_sources += 'steamshim/steamshim_child.c'
        steamworks = true
    endif
endif

# ====================
# Main source
# ====================

# Suppress warnings
global_args += ['-Wno-non-virtual-dtor', '-Wno-reorder', '-Wno-uninitialized', '-Wno-unknown-pragmas', '-Wno-stringop-truncation']
if compilers['cpp'].get_id() == 'clang'
    global_args += ['-Wno-undefined-var-template', '-Wno-delete-non-abstract-non-virtual-dtor']
endif

# Decide whether or not to use MiniFFI
miniffi = get_option('use_miniffi')
if miniffi == true
    miniffi = true
    global_args += '-DMKXPZ_MINIFFI'
endif

# Defines
if get_option('workdir_current')
    global_args += '-DWORKDIR_CURRENT'
endif

if get_option('cxx11_experimental') == true
    global_args += '-DMKXPZ_EXP_FS'
endif

if get_option('force32') == true
    global_args += '-m32'
endif

build_static = false
if get_option('static_executable') == true
    build_static = true
endif

global_args += '-DMKXPZ_INIT_GL_LATER'

subdir('src')
subdir('binding')
subdir('shader')
subdir('assets')

global_include_dirs += include_directories('src', 'binding')

rpath = ''

exe_name = meson.project_name()

if (host_system == 'linux' or host_system == 'darwin') and get_option('appimage') == false
    exe_name += '.' + host_machine.cpu_family()
endif

if host_system == 'windows'
    windows_resource_directory = '../' + get_option('windows_resource_directory')
    subdir('windows')
    global_include_dirs += include_directories('windows')
else
    windows_resources = []
endif

if host_system == 'darwin'
    global_dependencies += dependency(
        'appleframeworks',
        modules: [
            'AppKit',
            'Foundation',
            'GameController',
            'Metal',
        ],
        required: true,
    )
    subdir('macos/views')
endif

if steamworks == true
    exe_name = 'steam_' + exe_name
    la = ''
    if build_static == true
        if host_system == 'windows'
            la = '-static'
        else
            la = '-static-libgcc -static-libstdc++'
        endif
    endif

    shim_args = [
        '-DGAME_LAUNCH_NAME="' + exe_name + '"',
        '-I' + steamworks_path + '/public'
        ]

    if get_option('steam_appid') != ''
        shim_args += '-DSTEAM_APPID=' + get_option('steam_appid')
    endif

    if get_option('steamshim_debug') == true
        shim_args += '-DSTEAMSHIM_DEBUG'
        shim_ws = 'console'
    else
        shim_ws = 'windows'
    endif

    executable(meson.project_name(),
        sources: files('steamshim/steamshim_parent.cpp'),
        dependencies: steamlib,
        cpp_args: shim_args,
        link_args: la.split(),
        override_options: global_std,
        win_subsystem: shim_ws,
    )
endif

exe = executable(exe_name,
    sources: [
        git_hash_file, # Dummy source file just so that Meson doesn't complain about the executable not having any sources in non-Windows builds
        windows_resources,
    ],
    link_with: static_library(
        meson.project_name(),
        sources: global_sources,
        dependencies: global_dependencies,
        include_directories: global_include_dirs,
        c_args: global_args,
        cpp_args: global_args + global_cpp_args,
        objc_args: global_args + global_objc_args,
        objcpp_args: global_args + global_objc_args + global_cpp_args + global_objcpp_args,
        override_options: global_std,
    ),
    include_directories: global_include_dirs,
    link_args: global_link_args,
    win_subsystem: 'windows',
    install: true,
    install_dir: '.',
    install_tag: meson.project_name(),
)

global_install_tags = [meson.project_name()]
if host_system == 'darwin'
    global_install_tags += meson.project_name() + '-bundle'
endif

summary('ALSA', have_alsa, bool_yn: true)
summary('ANGLE', have_angle, bool_yn: true)
if have_angle
    summary('ANGLE Direct3D 9 backend', have_angle_direct3d9, bool_yn: true)
    summary('ANGLE Direct3D 11 backend', have_angle_direct3d11, bool_yn: true)
    summary('ANGLE Metal backend', have_angle_metal, bool_yn: true)
    summary('ANGLE null backend', have_angle_null, bool_yn: true)
    summary('ANGLE Vulkan backend', have_angle_vulkan, bool_yn: true)
endif
summary('Extra shaders', have_extra_shaders, bool_yn: true)
summary('JACK Audio Connection Kit', have_jack, bool_yn: true)
summary('OpenSSL', have_openssl, bool_yn: true)
summary('PipeWire', have_pipewire, bool_yn: true)
summary('PulseAudio', have_pulseaudio, bool_yn: true)
summary('License', gplv3_features.length() != 0 ? 'GPL-3.0-or-later' : 'GPL-2.0-or-later')
if gplv3_features.length() != 0
    summary('Enabled features requiring GPLv3', gplv3_features)
endif

foreach install_tag : global_install_tags
    install_data(
        gplv3_features.length() != 0 ? 'assets/LICENSE.mkxp-z-with-https.txt' : 'COPYING',
        rename: 'LICENSE.txt',
        install_dir: '.',
        install_tag: install_tag,
    )

    install_data(
        'mkxp.json',
        install_dir: '.',
        install_tag: install_tag,
    )

    install_subdir(
        'scripts',
        install_dir: '.',
        install_tag: install_tag,
    )

    if ruby.type_name() == 'internal'
        ruby_version = ruby.version().split('.')[0] + '.' + ruby.version().split('.')[1]
        meson.add_install_script(
            python,
            files('assets/install-directory.py'),
            meson.project_build_root() / 'subprojects' / ('ruby-' + ruby_version) / 'dist' / 'lib' / 'ruby' / (ruby_version + '.0'),
            'stdlib',
            install_tag: install_tag,
        )
    endif
endforeach

if host_system == 'darwin'
    meson.add_install_script(
        python,
        files('assets/install-executable.py'),
        exe,
        (exe_name + '.app') / 'Contents' / 'MacOS' / meson.project_name(),
        strip,
        install_tag: meson.project_name() + '-bundle',
    )

    configure_file(
        input: 'macos/Info.plist.in',
        output: 'Info.plist',
        configuration: {
            'CFBundleExecutable': meson.project_name(),
            'CFBundleGetInfoString': meson.project_name(),
            'CFBundleIdentifier': meson.project_name(),
            'CFBundleName': meson.project_name(),
            'CFBundleShortVersionString': meson.project_version(),
        },
        install: true,
        install_dir: (exe_name + '.app') / 'Contents',
        install_tag: meson.project_name() + '-bundle',
    )

    install_data(
        'macos/icon.icns',
        install_dir: (exe_name + '.app') / 'Contents' / 'Resources',
        install_tag: meson.project_name() + '-bundle',
    )
endif
