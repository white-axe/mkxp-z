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
#include <unordered_map>
#include <vector>

#include "hlsl-to-spirv.h"
#include "spirv-optimizer.h"
#include "spirv-to-glsl.h"

class Effectc
{
private:
    struct Program
    {
        uint32_t vertex_shader_index;
        uint32_t fragment_shader_index;
        uint32_t program_index;
        std::vector<SpirvToGlsl::Ubo> ubos;
        std::unordered_map<std::string, SpirvToGlsl::Uniform> uniforms;

        Program();
        ~Program();
        Program(const Program &other) = delete;
        Program(Program &&other) noexcept;
        Program &operator=(const Program &other) = delete;
        Program &operator=(Program &&other) noexcept;
    };

    std::unordered_map<std::string, Program> programs;

public:
    Effectc() = default;
    ~Effectc() = default;
    Effectc(const Effectc &other) = delete;
    Effectc(Effectc &&other) noexcept = default;
    Effectc &operator=(const Effectc &other) = delete;
    Effectc &operator=(Effectc &&other) noexcept = default;

    /* Compiles a Direct3D 9 effect. If successful, any shaders already present in this object will be deleted and replaced with the shaders from the newly compiled effect.
     *
     * error_messages: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing all compilation errors that occurred.
     * hlsl_to_spirv_compiler: Compiler to use to compile HLSL shaders to SPIR-V.
     * optimizer: Optimizer to use to optimize SPIR-V shaders.
     * spirv_to_glsl_compiler: Compiler to use to compile SPIR-V shaders to GLSL.
     * shaders_source_code: The HLSL source code for the shaders to be compiled.
     * passes_source_code: The HLSL source code for the passes describing the shaders to be compiled, as one or more Direct3D 9 passes; see <https://github.com/MicrosoftDocs/win32/blob/9bcde4a0658c5b315366da6124466465bb25e5ee/desktop-src/direct3d9/techniques-and-passes.md#passes>.
     * shaders_line_number_offset: Offset to add to the line numbers in error messages in the source code for the shaders. This can be useful if the source code for the shaders contains some kind of preamble.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(std::string &error_messages, HlslToSpirv &hlsl_to_spirv_compiler, SpirvOptimizer &optimizer, SpirvToGlsl &spirv_to_glsl_compiler, const char *shaders_source_code, const char *passes_source_code, int shaders_line_number_offset);
};

#endif // EFFECTC_H
