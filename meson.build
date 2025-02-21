project('mkxp-z', 'c', 'cpp', version: '2.4.2', meson_version: '>=1.3.0', default_options: ['cpp_std=c++14', 'buildtype=release', 'b_staticpic=false'])

host_system = host_machine.system()
host_endian = host_machine.endian()

is_libretro = get_option('libretro')
is_emscripten = host_system == 'emscripten'
core_is_static = is_emscripten or host_system == 'bare' or host_system == 'none'

if not is_libretro and host_system == 'darwin'
    error('This Meson project no longer supports macOS. Please use the Xcode project instead.')
endif

compilers = {'c': meson.get_compiler('c'), 'cpp': meson.get_compiler('cpp')}

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
global_args += '-DHAVE_NANOSLEEP'
# ====================
# Ext libs
# ====================

if is_libretro
    libretro_stage1_path = get_option('libretro_stage1_path')

    libretro_link_args = []
    libretro_cflags = []
    libretro_cppflags = []

    if core_is_static and not is_emscripten and get_option('b_lto')
        compilers['c'].has_argument('-ffat-lto-objects', required: true)
        compilers['cpp'].has_argument('-ffat-lto-objects', required: true)
        libretro_cflags += '-ffat-lto-objects'
        libretro_cppflags += '-ffat-lto-objects'
    endif

    # When targeting Emscripten, we need to build a relocatable object
    if is_emscripten
        compilers['cpp'].has_link_argument('-r', required: true)
        libretro_link_args += '-r'
    endif

    # We need to statically link the C++ standard library (libstdc++/libc++), the compiler runtime library (libgcc/compiler-rt) and libpthread in MSYS2 builds for Windows because those are not part of the operating system
    if (host_system == 'windows' or host_system == 'cygwin') and compilers['cpp'].has_link_argument('-static')
        libretro_link_args += '-static'
    endif

    # Android doesn't have a built-in C++ standard library, so we need to statically link against the C++ standard library
    if host_system == 'android'
        compilers['cpp'].has_link_argument('-static-libstdc++', required: true)
        libretro_link_args += '-static-libstdc++'
    endif

    # If possible, stop the linker from reexporting the symbols from the static libraries we use (e.g. zlib)
    if not is_emscripten and compilers['cpp'].has_link_argument('-Wl,--version-script,' + meson.current_source_dir() / 'libretro/link.T') # Only works with GNU linker and LLVM linker
        libretro_link_args += '-Wl,--version-script,' + meson.current_source_dir() / 'libretro/link.T'
    endif

    libretro_defines = [
        '-DMKXPZ_VERSION="@0@"'.format(meson.project_version()),
        '-DMKXPZ_RETRO',
        '-DSHARED_FLUID',
        '-D_FILE_OFFSET_BITS=64',
        '-DMPG123_NO_LARGENAME',
    ]

    if host_endian == 'big'
        libretro_defines += '-DMKXPZ_BIG_ENDIAN'
    endif

    if is_emscripten or not compilers['cpp'].compiles('struct E {}; int main() { throw E(); }', name: 'C++ exceptions support check')
        libretro_defines += '-DMKXPZ_NO_EXCEPTIONS'
        libretro_defines += '-DBOOST_NO_EXCEPTIONS'
    endif

    if not compilers['cpp'].has_header_symbol('mutex', 'std::mutex')
        libretro_defines += '-DMKXPZ_NO_STD_MUTEX'
    endif

    if not compilers['cpp'].has_header_symbol('mutex', 'std::recursive_mutex')
        libretro_defines += '-DMKXPZ_NO_STD_RECURSIVE_MUTEX'
    endif

    if not compilers['cpp'].has_header_symbol('thread', 'std::this_thread::yield')
        libretro_defines += '-DMKXPZ_NO_STD_THIS_THREAD_YIELD'
    endif

    if not compilers['cpp'].has_header('pthread.h') or not compilers['cpp'].compiles('''
        #include <pthread.h>
        int main() {
            pthread_mutex_t mutex;
            pthread_cond_t cond;
            pthread_mutex_init(&mutex, NULL);
            pthread_cond_init(&cond, NULL);
            pthread_cond_signal(&cond);
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&cond, &mutex);
            int result = pthread_self() != pthread_self();
            pthread_mutex_unlock(&mutex);
            pthread_cond_destroy(&cond);
            pthread_mutex_destroy(&mutex);
            return result;
        }
    ''', name: 'pthread.h sanity check')
        libretro_defines += '-DMKXPZ_NO_PTHREAD_H'
        libretro_defines += '-DMKXPZ_NO_SEMAPHORE_H'
    elif not compilers['cpp'].has_header('semaphore.h') or not compilers['cpp'].compiles('''
        #include <semaphore.h>
        int main() {
            sem_t sem;
            sem_init(&sem, 0, 0);
            sem_post(&sem);
            sem_wait(&sem);
            sem_destroy(&sem);
            return 0;
        }
    ''', name: 'semaphore.h sanity check')
        libretro_defines += '-DMKXPZ_NO_SEMAPHORE_H'
    endif

    # Enable integer-only mode in FLAC when building with Vita SDK. Otherwise, we get an internal compiler error in FLAC__window_triangle:
    #
    # ../subprojects/flac/src/libFLAC/window.c: In function 'FLAC__window_triangle':
    # ../subprojects/flac/src/libFLAC/window.c:197:1: error: unrecognizable insn:
    #   197 | }
    #       | ^
    # (insn 191 190 192 21 (set (reg:V4SF 443)
    #         (mult:V4SF (reg:V4SF 443)
    #             (reg:V4SF 444))) "../subprojects/flac/src/libFLAC/window.c":189:43 -1
    #      (nil))
    # during RTL pass: vregs
    # ../subprojects/flac/src/libFLAC/window.c:197:1: internal compiler error: in extract_insn, at recog.c:2294
    # Please submit a full bug report,
    # with preprocessed source if appropriate.
    # See <https://gcc.gnu.org/bugs/> for instructions.
    if compilers['c'].has_header_symbol('sys/config.h', '__vita__')
        libretro_defines += '-DFLAC__INTEGER_ONLY_LIBRARY'
    endif

    # Position-independent code is not supported on some platforms where we need to build a static library, e.g. https://github.com/vitasdk/vita-toolchain/issues/264
    use_pic = is_emscripten or not core_is_static
    if not use_pic and get_option('b_staticpic')
        error('The `b_staticpic` option needs to be disabled when building for this platform.')
    endif

    cmake = import('cmake')

    boost_options = cmake.subproject_options()
    boost_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_TESTING': false,
    })

    zlib_options = cmake.subproject_options()
    zlib_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'ZLIB_BUILD_EXAMPLES': false,
    })

    libzip_options = cmake.subproject_options()
    libzip_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
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
        'ENABLE_BZIP2': false,
        'ENABLE_LZMA': false,
        'ENABLE_ZSTD': false,
    })

    physfs_options = cmake.subproject_options()
    physfs_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'PHYSFS_BUILD_STATIC': true,
        'PHYSFS_BUILD_SHARED': false,
        'PHYSFS_BUILD_TEST': false,
        'PHYSFS_BUILD_DOCS': false,
    })

    openal_options = cmake.subproject_options()
    openal_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'LIBTYPE': 'STATIC',
        'ALSOFT_DLOPEN': false,
        'ALSOFT_UTILS': false,
        'ALSOFT_NO_CONFIG_UTIL': true,
        'ALSOFT_EXAMPLES': false,
        'ALSOFT_UPDATE_BUILD_VERSION': false,
        'ALSOFT_EMBED_HRTF_DATA': false,
        'ALSOFT_RTKIT': false,
        'ALSOFT_BACKEND_PIPEWIRE': false,
        'ALSOFT_BACKEND_PULSEAUDIO': false,
        'ALSOFT_BACKEND_ALSA': false,
        'ALSOFT_BACKEND_OSS': false,
        'ALSOFT_BACKEND_SOLARIS': false,
        'ALSOFT_BACKEND_SNDIO': false,
        'ALSOFT_BACKEND_WINMM': false,
        'ALSOFT_BACKEND_DSOUND': false,
        'ALSOFT_BACKEND_WASAPI': false,
        'ALSOFT_BACKEND_OTHERIO': false,
        'ALSOFT_BACKEND_JACK': false,
        'ALSOFT_BACKEND_COREAUDIO': false,
        'ALSOFT_BACKEND_OBOE': false,
        'ALSOFT_BACKEND_OPENSL': false,
        'ALSOFT_BACKEND_PORTAUDIO': false,
        'ALSOFT_BACKEND_SDL3': false,
        'ALSOFT_BACKEND_SDL2': false,
        'ALSOFT_BACKEND_WAVE': false,
    })

    fluidlite_options = cmake.subproject_options()
    fluidlite_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'FLUIDLITE_BUILD_STATIC': true,
        'FLUIDLITE_BUILD_SHARED': false,
    })

    ogg_options = cmake.subproject_options()
    ogg_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
        'BUILD_FRAMEWORK': false,
    })

    vorbis_options = cmake.subproject_options()
    vorbis_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
        'BUILD_FRAMEWORK': false,
    })

    flac_options = cmake.subproject_options()
    flac_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_CXXLIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_EXAMPLES': false,
        'BUILD_TESTING': false,
        'BUILD_DOCS': false,
        'BUILD_UTILS': false,
        'ENABLE_MULTITHREADING': false,
        'INSTALL_MANPAGES': false,
        'WITH_OGG': false,
        'WITH_FORTIFY_SOURCE': false,
        'WITH_STACK_PROTECTOR': false,
    })

    opus_options = cmake.subproject_options()
    opus_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'OPUS_BUILD_SHARED_LIBRARY': false,
        'OPUS_BUILD_TESTING': false,
        'OPUS_CUSTOM_MODES': false,
        'OPUS_BUILD_PROGRAMS': false,
        'OPUS_DISABLE_INTRINSICS': true,
        'OPUS_FLOAT_APPROX': false,
        'OPUS_BUILD_FRAMEWORK': false,
        'OPUS_STATIC_RUNTIME': false,
        'OPUS_FORTIFY_SOURCE': false,
        'OPUS_STACK_PROTECTOR': false,
    })

    mpg123_options = cmake.subproject_options()
    mpg123_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_LIBOUT123': false,
        'CHECK_MODULES': false,
        'FIFO': false,
    })

    libsndfile_options = cmake.subproject_options()
    libsndfile_options.add_cmake_defines({
        'CMAKE_C_FLAGS': ' '.join(libretro_defines + libretro_cflags),
        'CMAKE_CXX_FLAGS': ' '.join(libretro_defines + libretro_cppflags),
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_EXAMPLES': false,
        'BUILD_REGTEST': false,
        'BUILD_TESTING': false,
        'ENABLE_CPACK': false,
        'ENABLE_PACKAGE_CONFIG': false,
        'INSTALL_PKGCONFIG_MODULE': false,
        'ENABLE_EXTERNAL_LIBS': true,
        'ENABLE_MPEG': true,
        'ENABLE_EXPERIMENTAL': false,
    })

    libretro_deps = [
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
        cmake.subproject('libzip', options: libzip_options).dependency('zip'),
        cmake.subproject('physfs', options: physfs_options).dependency('physfs-static'),
        cmake.subproject('openal-soft', options: openal_options).dependency('OpenAL'),
        cmake.subproject('fluidlite', options: fluidlite_options).dependency('fluidlite-static'),
        cmake.subproject('ogg', options: ogg_options).dependency('ogg'),
        cmake.subproject('vorbis', options: vorbis_options).dependency('vorbis'),
        cmake.subproject('vorbis', options: vorbis_options).dependency('vorbisfile'),
        cmake.subproject('flac', options: flac_options).dependency('FLAC'),
        cmake.subproject('opus', options: opus_options).dependency('opus'),
        cmake.subproject('mpg123', options: mpg123_options).dependency('libmpg123'),
        cmake.subproject('libsndfile', options: libsndfile_options).dependency('sndfile'),
    ]
    if host_system == 'darwin'
        libretro_deps += compilers['cpp'].find_library('iconv')
    endif

    if core_is_static
        libretro_deps_processed = []
        foreach dep : libretro_deps
            libretro_deps_processed += dep.as_link_whole()
        endforeach
    else
        libretro_deps_processed = libretro_deps
    endif

    if is_emscripten
        libretro_target_type = 'executable' # Actually a relocatable object; we just pass 'executable' as the target type to get Meson to use the correct commands
    elif core_is_static
        libretro_target_type = 'static_library'
    else
        libretro_target_type = 'shared_library'
    endif

    build_target(
        meson.project_name() + '_libretro',
        name_prefix: '',
        name_suffix: is_emscripten ? 'bc' : [],
        target_type: libretro_target_type,
        dependencies: libretro_deps_processed,
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
        ] + libretro_cflags + libretro_defines,
        cpp_args: ['-Wno-unused-command-line-argument'] + libretro_cppflags + libretro_defines,
        link_args: libretro_link_args,
        gnu_symbol_visibility: 'hidden',
        install: true, # Prevents Meson from creating thin archives; see https://github.com/mesonbuild/meson/issues/9479
        include_directories: [
            include_directories('.'),
            include_directories('binding-sandbox'),
            include_directories('src'),
            include_directories('src/audio'),
            include_directories('src/crypto'),
            include_directories('src/display'),
            include_directories('src/display/gl'),
            include_directories('src/display/libnsgif'),
            include_directories('src/display/libnsgif/utils'),
            include_directories('src/etc'),
            include_directories('src/filesystem'),
            include_directories('src/filesystem/ghc'),
            include_directories('src/input'),
            include_directories('src/net'),
            include_directories('src/system'),
            include_directories('src/util'),
            include_directories('src/util/sigslot'),
            include_directories('src/util/sigslot/adapter'),
            include_directories(libretro_stage1_path),
            include_directories(libretro_stage1_path / 'mkxp-retro-ruby'),
            include_directories(libretro_stage1_path / 'sdl/include'),
        ],
        sources: [
            vcs_tag(command: ['git', 'rev-parse', '--short', 'HEAD'], fallback: 'unknown', input: 'src/git-hash.h.in', output: 'git-hash.h'),
            'src/core.cpp',
            'src/mkxp-threads.cpp',
            'src/sharedstate.cpp',
            'src/audio/alstream.cpp',
            'src/audio/audio.cpp',
            'src/audio/audiostream.cpp',
            'src/audio/fluid-fun.cpp',
            'src/audio/midisource.cpp',
            'src/audio/sndfilesource.cpp',
            'src/audio/soundemitter.cpp',
            'src/audio/vorbissource.cpp',
            'src/crypto/rgssad.cpp',
            'src/display/bitmap.cpp',
            'src/display/plane.cpp',
            'src/display/sprite.cpp',
            'src/display/tilemap.cpp',
            'src/display/viewport.cpp',
            'src/display/window.cpp',
            'src/display/gl/scene.cpp',
            'src/display/gl/tileatlas.cpp',
            'src/etc/etc.cpp',
            'src/etc/table.cpp',
            'src/filesystem/filesystem.cpp',
            'src/input/input-retro.cpp',
            'binding-sandbox/binding-base.cpp',
            'binding-sandbox/sandbox.cpp',
            'binding-sandbox/wasi.cpp',
            'binding-sandbox/wasm-rt.cpp',
            'binding/module_rpg.cpp',
            libretro_stage1_path / 'GMGSx.sf2.cpp',
            libretro_stage1_path / 'mkxp-sandbox-bindgen.cpp',
            libretro_stage1_path / 'mkxp-retro-dist.zip.cpp',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_0.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_1.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_2.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_3.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_4.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_5.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_6.c',
            libretro_stage1_path / 'mkxp-retro-ruby/mkxp-retro-ruby_7.c',
        ],
    )
else

global_sources += vcs_tag(command: ['git', 'rev-parse', '--short', 'HEAD'], fallback: 'unknown', input: 'src/git-hash.h.in', output: 'git-hash.h')
if host_endian == 'big'
    global_args += '-DMKXPZ_BIG_ENDIAN'
endif
if is_emscripten or not compilers['cpp'].compiles('struct E {}; int main() { throw E(); }', name: 'check if C++ exceptions are enabled')
    libretro_defines += '-DMKXPZ_NO_EXCEPTIONS'
    libretro_defines += '-DBOOST_NO_EXCEPTIONS'
endif

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
