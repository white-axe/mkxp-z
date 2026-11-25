/*
** spirv-sink.h
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

#ifndef SPIRV_SINK_H
#define SPIRV_SINK_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include "spirv.h"
#include "spirv-optimizer.h"
#include "spirv-to-glsl.h"

class SpirvSink
{
public:
    enum Backend
    {
        // OpenGL backend with GLSL as the shading language.
        OPENGL_GLSL,
    };

private:
    void *context;
    Backend backend;

    SpirvOptimizer optimizer;

    union
    {
        SpirvToGlsl glsl_backend;
    };

public:
    class Program
    {
    private:
        friend class SpirvSink;

        bool initialized;

        struct Shader
        {
            SpirvShaderInfo info;
        };

        std::unordered_map<SpirvExecutionModel, SpirvShaderInfo> shaders;

        Backend backend;

        struct Gl
        {
            uint32_t program_index;
            std::vector<uint32_t> ubo_indices;
        };

        union
        {
            Gl gl;
        };

    public:
        Program() noexcept;
        ~Program();
        Program(const Program &other) = delete;
        Program(Program &&other) noexcept;
        Program &operator=(const Program &other) = delete;
        Program &operator=(Program &&other) noexcept;
    };

    SpirvSink(Backend backend);
    ~SpirvSink();
    SpirvSink(const SpirvSink &other) = delete;
    SpirvSink(SpirvSink &&other) noexcept;
    SpirvSink &operator=(const SpirvSink &other) = delete;
    SpirvSink &operator=(SpirvSink &&other) noexcept;

    /*
     * Returns the maximum SPIR-V target that this sink supports.
     */
    SpirvTarget target() const noexcept;

    /*
     * Creates a shader program from SPIR-V shaders.
     * You must specify, at minimum, a vertex shader and a fragment shader.
     *
     * shaders: For each SPIR-V shader, this contains the shader's SPIR-V bytecode and the length of the shader's SPIR-V bytecode in bytes.
     * shaders_len: The length of `shaders`.
     * program_output: If compilation succeeds, this will be set to the compiled shader program. Otherwise, this will be unchanged.
     * error_message: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing the compilation error.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(
        const std::pair<const uint32_t *, size_t> *shaders,
        size_t shaders_len,
        Program &program_output,
        std::string &error_message
    );
};

#endif // SPIRV_SINK_H
