binding_sandbox_sources = files(
    'audio-binding.cpp',
    'binding-base.cpp',
    'binding-sandbox.cpp',
    'binding-util.cpp',
    'bitmap-binding.cpp',
    'etc-binding.cpp',
    'font-binding.cpp',
    'graphics-binding.cpp',
    'input-binding.cpp',
    'module_rpg.cpp',
    'plane-binding.cpp',
    'sandbox.cpp',
    'sandbox-serial-util.cpp',
    'sprite-binding.cpp',
    'table-binding.cpp',
    'tilemap-binding.cpp',
    'tilemapvx-binding.cpp',
    'viewport-binding.cpp',
    'wasi.cpp',
    'wasm-rt.cpp',
    'window-binding.cpp',
    'windowvx-binding.cpp',
)

global_sources += binding_sandbox_sources

binding_sandbox_hasher = executable(
    'binding-sandbox-hasher',
    dependencies: subproject('picosha2').get_variable('picosha2'),
    sources: 'hasher.cpp',
    native: true,
    override_options: [
        'buildtype=release',
        'b_coverage=false',
        'b_lto=false',
        'b_ndebug=false',
        'b_pgo=off',
        'b_sanitize=none',
        'cpp_std=c++11',
    ],
)

global_sources += custom_target(
    'binding-sandbox-hash',
    input: binding_sandbox_sources,
    output: 'binding-sandbox-hash.h',
    command: [
        binding_sandbox_hasher,
        '@OUTPUT@',
        '@INPUT@',
    ],
)
