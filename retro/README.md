The build process for the libretro core is divided into two phases.

# Phase 1

All the files produced by this phase are platform-agnostic, so you can run this build phase on any computer, regardless of which operating system or CPU architecture the libretro core is for.

Download [WASI SDK](https://github.com/WebAssembly/wasi-sdk), [Binaryen](https://github.com/WebAssembly/binaryen) and [WABT](https://github.com/WebAssembly/wabt). Currently you must use WASI SDK version 21; later versions of the WASI SDK do not work. Binaryen and WABT versions can be arbitrary as long as they're relatively recent. You also need to have either [Universal Ctags](https://github.com/universal-ctags/ctags) or [Exuberant Ctags](https://ctags.sourceforge.net) installed, which we use for generating bindings for the Ruby C API.

Then go to the directory that this README.md is in and run this command, filling in the paths to WASI SDK, `wasm-opt` from Binaryen, `wasm2c` from WABT and `ctags` from Universal Ctags or Exuberant Ctags accordingly:

```
make WASI_SDK=/path/to/wasi-sdk WASM_OPT=/path/to/binaryen/bin/wasm-opt WASM2C=/path/to/wabt/bin/wasm2c CTAGS=/path/to/ctags
```

This will produce the directory "retro/build/retro-phase1".

# Phase 2

This phase produces the actual core file. Go to the root directory of this repository and run:

```
meson setup build -Dretro=true -Dretro_phase1_path=path/to/retro-phase1
cd build
ninja
```
