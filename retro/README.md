The build process for the libretro core is divided into two phases.

# Phase 1

All the files produced by this phase are platform-agnostic, so you can run this build phase on any computer, regardless of which operating system or CPU architecture the libretro core is for.

Download [WASI SDK](https://github.com/WebAssembly/wasi-sdk), [Binaryen](https://github.com/WebAssembly/binaryen) and [WABT](https://github.com/WebAssembly/wabt). Currently you must use WASI SDK version 21; later versions of the WASI SDK do not work. Binaryen and WABT versions can be arbitrary as long as they're relatively recent.

Then go to the "retro" directory and run this command, filling in the paths to WASI SDK, `wasm-opt` from Binaryen and `wasm2c` from WABT accordingly:

```
make WASI_SDK=/path/to/wasi-sdk WASM_OPT=/path/to/binaryen/bin/wasm-opt WASM2C=/path/to/wabt/bin/wasm2c
```

This will produce the directory "retro/build/retro-phase1".

# Phase 2

This phase produces the actual core file.

```
meson setup build -Dretro=true -Dretro_phase1_path=/path/to/retro-phase1
cd build
ninja
```
