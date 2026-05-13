project('mkxp-z', 'c', 'cpp', version: '2.4.2', meson_version: '>=1.10.0', default_options: ['cpp_std=c++14', 'buildtype=release'])

host_system = host_machine.system()
host_endian = host_machine.endian()

use_pic = true

compilers = {'cpp': meson.get_compiler('cpp')}

git_hash_file = vcs_tag(command: ['git', 'rev-parse', '--short=7', 'HEAD'], fallback: 'unknown', input: 'src/git-hash.h.in', output: 'git-hash.h')

global_sources = []
global_dependencies = [declare_dependency(sources: git_hash_file)]
global_include_dirs = []
global_args = []
global_cpp_args = []
global_link_args = []

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

subdir('tools')
subdir('src')
subdir('binding')
subdir('shader')
subdir('assets')

global_include_dirs += include_directories('src', 'binding')

rpath = ''

if host_system == 'windows'
    windows_resource_directory = '../' + get_option('windows_resource_directory')
    subdir('windows')
    global_sources += windows_resources
    global_include_dirs += include_directories('windows')
endif

exe_name = meson.project_name()

if (host_system == 'linux' or host_system == 'darwin') and get_option('appimage') == false
    exe_name += '.' + host_machine.cpu_family()
endif

meson.override_dependency(
    'mkxp-global-dependencies',
    declare_dependency(
        dependencies: global_dependencies,
        include_directories: global_include_dirs,
    ),
    static: true,
)

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
        win_subsystem: shim_ws,
        install: (host_system != 'windows'))
endif

if host_system == 'darwin'
    apple_impl_options = {
        'objcppflags': global_args + global_cpp_args,
    }
    global_dependencies = [subproject('apple-impl', default_options: apple_impl_options).get_variable('apple_impl')] + global_dependencies
endif

executable(exe_name,
    sources: git_hash_file, # Dummy source file just so that Meson doesn't complain about the executable not having any sources
    link_with: static_library(
        meson.project_name(),
        sources: global_sources,
        dependencies: global_dependencies,
        include_directories: global_include_dirs,
        cpp_args: global_args + global_cpp_args,
        objc_args: global_args,
        objcpp_args: global_args + global_cpp_args,
    ),
    link_args: global_link_args,
    install_rpath: rpath,
    win_subsystem: 'windows',
    install: (host_system != 'windows')
)
