The build process for the libretro core is divided into two stages.

# Stage 1

All the files produced by this stage are platform-agnostic, so you can run this build stage on any computer, regardless of which operating system or CPU architecture the libretro core is for.

Required software:
* C and C++ compilers
* GNU Make
* GNU Autotools
* GNU Bison
* [Git](https://git-scm.com)
* [curl](https://curl.se)
* [Info-ZIP's Zip](https://infozip.sourceforge.net/Zip.html) (the `zip` package found in many package managers)
* [WASI SDK](https://github.com/WebAssembly/wasi-sdk) (currently you need WASI SDK version 21; later versions don't work yet)
* [Binaryen](https://github.com/WebAssembly/binaryen)
* [WABT](https://github.com/WebAssembly/wabt)
* Either [Universal Ctags](https://github.com/universal-ctags/ctags) or [Exuberant Ctags](https://ctags.sourceforge.net)

Go to the directory that this README.md is in and run this command, filling in the paths to WASI SDK, `wasm-opt` from Binaryen, `wasm2c` from WABT and `ctags` from Universal Ctags or Exuberant Ctags accordingly:

```
make WASI_SDK=/path/to/wasi-sdk WASM_OPT=/path/to/binaryen/bin/wasm-opt WASM2C=/path/to/wabt/bin/wasm2c CTAGS=/path/to/ctags
```

This will produce the directory "libretro/build/libretro-stage1".

# Stage 2

This stage produces the actual core file.

Required software:
* C and C++ compilers
* [Git](https://git-scm.com)
* [Meson](https://mesonbuild.com)
* [Ninja](https://ninja-build.org)
* [CMake](https://cmake.org)

No software libraries are required other than the system libraries.

Go to the root directory of this repository and run:

```
meson setup build -Dlibretro=true
cd build
ninja
```
