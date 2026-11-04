/*
** hlsl-to-spirv.h
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

#ifndef HLSL_TO_SPIRV_H
#define HLSL_TO_SPIRV_H

#include <cstdint>
#include <string>
#include <vector>

class HlslToSpirv
{
private:
    bool initialized;

public:
    HlslToSpirv();
    ~HlslToSpirv();
    HlslToSpirv(const HlslToSpirv &other) = delete;
    HlslToSpirv(HlslToSpirv &&other) noexcept;
    HlslToSpirv &operator=(const HlslToSpirv &other) = delete;
    HlslToSpirv &operator=(HlslToSpirv &&other) noexcept;

    enum Stage
    {
        // Vertex shader
        VERTEX,
        // Fragment shader
        FRAGMENT,
    };

    enum Dialect
    {
        // Compatible with Direct3D 9
        D3D9,
        // Compatible with Direct3D 10 and later
        MODERN,
    };

    /*
     * Compiles a HLSL shader to SPIR-V.
     *
     * spirv_output: If compilation succeeds, this will be set to the SPIR-V code that the HLSL compiled to. Otherwise this will be unchanged.
     * error_messages: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing all compilation errors that occurred.
     * hlsl_source_code: The HLSL source code for the shader to be compiled.
     * entry_point: The name of the main function of the shader.
     * stage: The type of shader that we are compiling (e.g. a vertex shader or fragment shader).
     * dialect: What type of HLSL the source code is written in.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(std::vector<uint8_t> &spirv_output, std::string &error_messages, const char *hlsl_source_code, const char *entry_point, Stage stage, Dialect dialect = Dialect::MODERN);
};

#endif // HLSL_TO_SPIRV_H
