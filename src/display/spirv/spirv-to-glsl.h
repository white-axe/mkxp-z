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

class SpirvToGlsl
{
private:
    void *context;

public:
    // Maximum SPIR-V target supported by this compiler backend.
    static constexpr uint32_t TARGET = 0x00010600;

    SpirvToGlsl();
    ~SpirvToGlsl();
    SpirvToGlsl(const SpirvToGlsl &other) = delete;
    SpirvToGlsl(SpirvToGlsl &&other) noexcept;
    SpirvToGlsl &operator=(const SpirvToGlsl &other) = delete;
    SpirvToGlsl &operator=(SpirvToGlsl &&other) noexcept;

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
     * You must check the validity of the input SPIR-V module yourself before passing it to this function; it's undefined behavior to pass an invalid SPIR-V module to this function.
     *
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in 32-bit words.
     * stage_input_cookie: Due to the limitations of old GLSL versions, you must provide an unpredictable string here, suitable for use in an identifier, that is the same as the `stage_output_cookie` used to compile the previous shader stage. This string is ignored for vertex shaders since there is no shader stage before the vertex stage.
     * stage_output_cookie: Due to the limitations of old GLSL versions, you must provide an unpredictable string here, suitable for use in an identifier, that is the same as the `stage_input_cookie` used to compile the next shader stage. Must not be the same as `stage_input_cookie`. This string is ignored for fragment shaders since there is no shader stage after the fragment stage.
     * version: The 3-digit GLSL version number, such as 460 for GLSL as used in OpenGL 4.6.
     * dialect: Whether to target desktop GLSL or GLSL ES.
     * glsl_output: If compilation succeeds, this will be set to the compiled GLSL shader. Otherwise this will be unchanged.
     * error_message: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing the compilation error.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(
        const uint32_t *spirv_input,
        size_t spirv_input_size,
        const char *stage_input_cookie,
        const char *stage_output_cookie,
        uint16_t version,
        Dialect dialect,
        std::string &glsl_output,
        std::string &error_message
    );
};

#endif // SPIRV_TO_GLSL_H
