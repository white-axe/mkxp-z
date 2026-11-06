/*
** spirv-to-glsl.h
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

#ifndef SPIRV_TO_GLSL_H
#define SPIRV_TO_GLSL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class SpirvToGlsl
{
private:
    void *context;

public:
    SpirvToGlsl();
    ~SpirvToGlsl();
    SpirvToGlsl(const SpirvToGlsl &other) = delete;
    SpirvToGlsl(SpirvToGlsl &&other) noexcept;
    SpirvToGlsl &operator=(const SpirvToGlsl &other) = delete;
    SpirvToGlsl &operator=(SpirvToGlsl &&other) noexcept;

    struct Ubo
    {
        // The size of this UBO in bytes.
        size_t size;
        // The OpenGL binding point that this UBO should be bound to before the GLSL shader is run.
        // If binding points are not supported by the combination of the GLSL version and dialect, this will always be zero.
        uint32_t binding;
    };

    enum Dialect
    {
        // GLSL for a desktop OpenGL implementation that supports the ARB_shading_language_420pack OpenGL extension
        GLSL_WITH_420PACK,
        // GLSL for a desktop OpenGL implementation that doesn't support the ARB_shading_language_420pack OpenGL extension
        GLSL_WITHOUT_420PACK,
        // GLSL for OpenGL ES
        GLSL_ES,
    };

    /*
     * Compiles SPIR-V bytecode to GLSL.
     *
     * glsl_output: If compilation succeeds, this will be set to the compiled GLSL shader. Otherwise this will be unchanged.
     * ubos_output: If compilation succeeds, this will be set to the list of descriptions of the uniform buffer objects (UBOs) used by the compiled GLSL shader. Otherwise this will be unchanged. If the combination of the GLSL version and dialect does not support UBOs, any UBOs used by the input SPIR-V shader will be automatically demoted to regular uniforms in the compiled GLSL shader for you, leaving the list of UBOs empty.
     * error_message: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing the compilation error.
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in bytes.
     * version: The 3-digit GLSL version number, such as 460 for GLSL as used in OpenGL 4.6.
     * dialect: Whether to target desktop GLSL or GLSL ES.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(std::string &glsl_output, std::vector<Ubo> &ubos_output, std::string &error_message, const uint8_t *spirv_input, size_t spirv_input_size, uint16_t version, Dialect dialect);
};

#endif // SPIRV_TO_GLSL_H
