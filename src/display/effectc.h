/*
** effectc.h
**
** This file is part of mkxp.
**
** Copyright (C) 2026 The mkxp-z authors
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EFFECTC_H
#define EFFECTC_H

#include <cstdint>

#include "hlsl-to-spirv.h"
#include "spirv-sink.h"

namespace effectc {
    /* Compiles a Direct3D 9 effect. If successful, any shaders already present in this object will be deleted and replaced with the shaders from the newly compiled effect.
     *
     * programs_output: If compilation succeeds, this will be set to a mapping between a pass name in the Direct3D 9 effect and the corresponding shader program. Otherwise, this will be unchanged.
     * error_messages: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing all compilation errors that occurred.
     * hlsl_to_spirv_compiler: Compiler to use to compile HLSL shaders to SPIR-V.
     * sink: Compiler to use to compile SPIR-V programs.
     * default_vertex_shader: SPIR-V bytecode for the vertex shader to use for a pass if a pass doesn't specify one. Can be null, in which case there will be an error if a pass doesn't specify a vertex shader.
     * default_vertex_shader_size: Length of `default_vertex_shader` in 32-bit words.
     * override_vertex_shader: If true, `default_vertex_shader` will be used even if the pass already specifies a vertex shader.
     * default_fragment_shader: SPIR-V bytecode for the fragment shader to use for a pass if a pass doesn't specify one. Can be null, in which case there will be an error if a pass doesn't specify a fragment shader.
     * default_fragment_shader_size: Length of `default_vertex_shader` in 32-bit words.
     * override_fragment_shader: If true, `default_fragment_shader` will be used even if the pass already specifies a fragment shader.
     * shaders_source_code: The HLSL source code for the shaders to be compiled.
     * passes_source_code: The HLSL source code for the passes describing the shaders to be compiled, as one or more Direct3D 9 passes; see <https://github.com/MicrosoftDocs/win32/blob/9bcde4a0658c5b315366da6124466465bb25e5ee/desktop-src/direct3d9/techniques-and-passes.md#passes>.
     * shaders_preamble_size: The number of bytes at the beginning of the source code for the shaders that should be considered the "preamble". The line numbers in shader error messages will be adjusted to exclude the preamble.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(
        std::unordered_map<std::string, SpirvSink::Program> &programs_output,
        std::string &error_messages,
        HlslToSpirv &hlsl_to_spirv_compiler,
        SpirvSink &sink,
        const uint32_t *default_vertex_shader,
        size_t default_vertex_shader_size,
        bool override_vertex_shader,
        const uint32_t *default_fragment_shader,
        size_t default_fragment_shader_size,
        bool override_fragment_shader,
        const char *shaders_source_code,
        const char *passes_source_code,
        size_t shaders_preamble_size
    );
}

#endif // EFFECTC_H
