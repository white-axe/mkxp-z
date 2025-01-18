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

This phase produces the actual core file. You need to have [Meson](https://mesonbuild.com), [Ninja](https://ninja-build.org), [CMake](https://cmake.org), [Git](https://git-scm.com), a C compiler and a C++ compiler. No software libraries are required other than the system libraries.

Go to the root directory of this repository and run:

```
meson setup build -Dretro=true -Dretro_phase1_path=path/to/retro-phase1
cd build
ninja
```

If you have a program named `patch` in your PATH, it has to be GNU patch. If your `patch` program isn't GNU patch (e.g. macOS comes with its own incompatible version of `patch` that will break this build system), either install GNU patch and then temporarily add it to your PATH for the duration of the `meson setup` command, or temporarily remove your `patch` program from the PATH for the duration of the `meson setup` command: the build system will fall back to using `git apply` if `patch` is not found, which will work fine.
