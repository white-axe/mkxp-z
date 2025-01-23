project('mkxp-z', 'c', 'cpp', version: '2.4.2', meson_version: '>=1.3.0', default_options: ['cpp_std=c++14', 'buildtype=release'])

host_system = host_machine.system()
host_endian = host_machine.endian()

if get_option('retro') == false and host_system == 'darwin'
    error('\nThis Meson project no longer supports macOS. Please use the Xcode project instead.')
endif

git_hash = run_command('git', 'rev-parse', '--short', 'HEAD').stdout().strip()

compilers = {'cpp': meson.get_compiler('cpp')}

global_sources = []
global_dependencies = []
global_include_dirs = []
global_args = []
global_link_args = []

sizeof = {'void*': compilers['cpp'].sizeof('void*'),
          'long': compilers['cpp'].sizeof('long')
         }
win64 = (sizeof['void*'] != sizeof['long'])

global_args += '-DMKXPZ_BUILD_MESON'
global_args += '-DMKXPZ_VERSION="@0@"'.format(meson.project_version())
global_args += '-DMKXPZ_GIT_HASH="@0@"'.format(git_hash)
global_args += '-DHAVE_NANOSLEEP'
# ====================
# Ext libs
# ====================

if get_option('retro') == true
    retro_phase1 = get_option('retro_phase1_path')

    cmake = import('cmake')

    boost_options = cmake.subproject_options()
    boost_options.add_cmake_defines({
        'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
        'BUILD_TESTING': false,
    })

    zlib_options = cmake.subproject_options()
    zlib_options.add_cmake_defines({
        'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
        'BUILD_SHARED_LIBS': false,
        'ZLIB_BUILD_EXAMPLES': false,
    })

    bzip2_options = cmake.subproject_options()
    bzip2_options.add_cmake_defines({
        'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
        'CMAKE_BUILD_TYPE': 'None',
        'ENABLE_STATIC_LIB': true,
        'ENABLE_SHARED_LIB': false,
        'ENABLE_LIB_ONLY': true,
    })

    liblzma_options = cmake.subproject_options()
    liblzma_options.add_cmake_defines({
        'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
        'BUILD_SHARED_LIBS': false,
        'ENABLE_NLS': false,
        'ENABLE_THREADS': 'OFF',
    })

    zstd_options = cmake.subproject_options()
    zstd_options.add_cmake_defines({
        'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
        'ZSTD_BUILD_STATIC': true,
        'ZSTD_BUILD_SHARED': false,
        'ZSTD_BUILD_PROGRAMS': false,
        'ZSTD_BUILD_TESTS': false,
        'ZSTD_BUILD_CONTRIB': false,
        'ZSTD_MULTITHREAD_SUPPORT': false,
        'ZSTD_LEGACY_SUPPORT': false,
    })

    libzip_options = cmake.subproject_options()
    libzip_options.add_cmake_defines({
        'CMAKE_POSITION_INDEPENDENT_CODE': get_option('b_staticpic'),
        'BUILD_SHARED_LIBS': false,
        'LIBZIP_DO_INSTALL': false,
        'BUILD_TOOLS': false,
        'BUILD_REGRESS': false,
        'BUILD_OSSFUZZ': false,
        'BUILD_EXAMPLES': false,
        'BUILD_DOC': false,
        'ENABLE_COMMONCRYPTO': false,
        'ENABLE_GNUTLS': false,
        'ENABLE_MBEDTLS': false,
        'ENABLE_OPENSSL': false,
        'ENABLE_WINDOWS_CRYPTO': false,
        'ENABLE_FDOPEN': false,
        'ENABLE_BZIP2': true,
        'ENABLE_LZMA': true,
        'ENABLE_ZSTD': true,
    })

    retro_defines = [
        '-DWASM_RT_SKIP_SIGNAL_RECOVERY',
        '-DWASM_RT_TRAP_HANDLER=mkxp_sandbox_trap_handler',
        '-DMKXPZ_RETRO',
    ]
    if host_endian == 'big'
        retro_defines += '-DWABT_BIG_ENDIAN=1'
    endif

    retro_link_args = []

    # We need to statically link the C++ standard library (libstdc++/libc++), the compiler runtime library (libgcc/compiler-rt) and libpthread in MSYS2 builds for Windows because those are not part of the operating system
    if (host_system == 'windows' or host_system == 'cygwin') and compilers['cpp'].has_link_argument('-static')
        retro_link_args += '-static'
    endif

    # Android doesn't have a built-in C++ standard library, so we need to statically link against the C++ standard library
    if host_system == 'android'
        compilers['cpp'].has_link_argument('-static-libstdc++', required: true)
        retro_link_args += '-static-libstdc++'
    endif

    # If possible, stop the linker from reexporting the symbols from the static libraries we use (e.g. zlib)
    if compilers['cpp'].has_link_argument('-Wl,--version-script,' + join_paths(meson.current_source_dir(), 'retro/link.T')) # Only works with GNU linker and LLVM linker
        retro_link_args += '-Wl,--version-script,' + join_paths(meson.current_source_dir(), 'retro/link.T')
    endif

    library(
        'retro-' + meson.project_name(),
        dependencies: [
            cmake.subproject('boost_asio', options: boost_options).dependency('boost_asio'),
            cmake.subproject('boost_mp11', options: boost_options).dependency('boost_mp11'),
            cmake.subproject('boost_describe', options: boost_options).dependency('boost_describe'),
            cmake.subproject('boost_config', options: boost_options).dependency('boost_config'),
            cmake.subproject('boost_assert', options: boost_options).dependency('boost_assert'),
            cmake.subproject('boost_static_assert', options: boost_options).dependency('boost_static_assert'),
            cmake.subproject('boost_throw_exception', options: boost_options).dependency('boost_throw_exception'),
            cmake.subproject('boost_core', options: boost_options).dependency('boost_core'),
            cmake.subproject('boost_container_hash', options: boost_options).dependency('boost_container_hash'),
            cmake.subproject('boost_type_index', options: boost_options).dependency('boost_type_index'),
            cmake.subproject('boost_type_traits', options: boost_options).dependency('boost_type_traits'),
            cmake.subproject('boost_optional', options: boost_options).dependency('boost_optional'),
            cmake.subproject(host_system == 'darwin' ? 'zlib-darwin' : 'zlib', options: zlib_options).dependency('zlibstatic'),
            cmake.subproject('bzip2', options: bzip2_options).dependency('bz2_static'),
            cmake.subproject('liblzma', options: liblzma_options).dependency('liblzma'),
            cmake.subproject('zstd', options: zstd_options).dependency('libzstd_static'),
            cmake.subproject('libzip', options: libzip_options).dependency('zip'),
        ],
        c_args: [
            '-fno-optimize-sibling-calls',
            '-frounding-math',
            '-fsignaling-nans',
            '-Wno-unused-function',
            '-Wno-unused-value',
            '-Wno-unused-variable',
            '-Wno-unused-but-set-variable',
            '-Wno-ignored-optimization-argument',
            '-Wno-unused-command-line-argument',
        ] + retro_defines,
        cpp_args: ['-Wno-unused-command-line-argument'] + retro_defines,
        link_args: retro_link_args,
        gnu_symbol_visibility: 'hidden',
        include_directories: [
            include_directories('.'),
            include_directories('src'),
            include_directories('src/display'),
            include_directories('src/display/gl'),
            include_directories('src/etc'),
            include_directories('src/util'),
            include_directories(retro_phase1),
            include_directories(join_paths(retro_phase1, 'wasm2c')),
            include_directories(join_paths(retro_phase1, 'mkxp-retro-ruby')),
        ],
        sources: [
            'src/core.cpp',
            'src/sharedstate.cpp',
            'src/display/bitmap.cpp',
            'src/display/viewport.cpp',
            'src/display/window.cpp',
            'src/display/gl/scene.cpp',
            'src/etc/etc.cpp',
            'src/etc/table.cpp',
            'binding-sandbox/binding-util.cpp',
            'binding-sandbox/sandbox.cpp',
            'binding-sandbox/wasi.cpp',
            'binding/module_rpg.cpp',
            join_paths(retro_phase1, 'wasm2c/wasm-rt-impl.c'),
            join_paths(retro_phase1, 'wasm2c/wasm-rt-mem-impl.c'),
            join_paths(retro_phase1, 'mkxp-sandbox-bindgen.cpp'),
            join_paths(retro_phase1, 'mkxp-retro-dist.zip.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_0.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_1.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_2.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_3.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_4.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_5.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_6.c'),
            join_paths(retro_phase1, 'mkxp-retro-ruby/mkxp-retro-ruby_7.c'),
        ],
    )
else

xxd = find_program('xxd', native: true)

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

# GLES
gfx_backend = get_option('gfx_backend')
if gfx_backend == 'gles'
    # Needs to be manually set up for now
    global_args += '-DGLES2_HEADER'
elif gfx_backend == 'gl'
    global_dependencies += dependency('gl')
    # boop
endif

# ====================
# Main source
# ====================

# Suppress warnings
global_args += ['-Wno-non-virtual-dtor', '-Wno-reorder', '-Wno-uninitialized', '-Wno-unknown-pragmas', '-Wno-stringop-truncation']
if compilers['cpp'].get_id() == 'clang'
    global_args += ['-Wno-undefined-var-template', '-Wno-delete-non-abstract-non-virtual-dtor']
endif
if host_system == 'windows'
    if compilers['cpp'].get_id() != 'clang'
        global_args += '-masm=intel'
    endif
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

if host_system == 'windows'
    windows_resource_directory = '../' + get_option('windows_resource_directory')
    subdir('windows')
    global_sources += windows_resources
    global_include_dirs += include_directories('windows')
else
    subdir('linux')
    rpath = '$ORIGIN/lib'
    if get_option('appimage') != true
        if sizeof['long'] == 8 and get_option('force32') != true
            rpath += '64'
        else
            rpath += '32'
        endif
    endif
endif

exe_name = meson.project_name()

if host_system == 'linux' and get_option('appimage') == false
    exe_name += '.' + host_machine.cpu()
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
        win_subsystem: shim_ws,
        install: (host_system != 'windows'))
endif

executable(exe_name,
    sources: global_sources,
    dependencies: global_dependencies,
    include_directories: global_include_dirs,
    install_rpath: rpath,
    link_args: global_link_args,
    cpp_args: global_args,
    objc_args: global_args,
    objcpp_args: global_args,
    win_subsystem: 'windows',
    install: (host_system != 'windows')
)

endif
