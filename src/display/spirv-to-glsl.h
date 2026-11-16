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
#include <unordered_map>
#include <vector>

#include "spirv-optimizer.h"

class SpirvToGlsl
{
private:
    void *context;

public:
    // Maximum SPIR-V target supported by this compiler backend.
    static constexpr SpirvOptimizer::Target TARGET = SpirvOptimizer::Target::SPIRV_1_6;

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

    struct Uniform
    {
        enum Type
        {
            // Unknown type.
            UNKNOWN,
            // 8-bit signed integer.
            INT8,
            // 8-bit unsigned integer.
            UINT8,
            // 16-bit signed integer.
            INT16,
            // 16-bit unsigned integer.
            UINT16,
            // 32-bit signed integer.
            INT32,
            // 32-bit unsigned integer.
            UINT32,
            // 64-bit signed integer.
            INT64,
            // 64-bit unsigned integer.
            UINT64,
            // IEEE 754 binary16 floating-point number.
            FP16,
            // IEEE 754 binary32 floating-point number.
            FP32,
            // IEEE 754 binary64 floating-point number.
            FP64,
        };

        enum Flavor
        {
            // This uniform is a member of a UBO.
            UBO,
            // This uniform is a plain uniform, not part of a UBO.
            PLAIN,
        };

        // The scalar type of this uniform.
        Type type;
        // `vector_size` and `columns` can each either be 1, 2, 3 or 4.
        // If both `vector_size` and `columns` are equal to 1, then this uniform is of scalar type.
        // If `vector_size` is greater than 1 and `columns` is equal to 1, then this uniform is a vector of `type` with `vector_size` elements.
        // If `columns` is greater than 1, then this uniform is a matrix of `type` with `vector_size` rows and `columns` columns.
        uint8_t vector_size, columns;
        // If non-empty, this uniform is an array, possibly multidimensional, with these dimensions.
        // It's possible for a uniform to be both of array type and of matrix or vector type at the same time, in which case the uniform is an array of matrices/vectors, not a matrix/vector of arrays.
        std::vector<uint32_t> array_dimensions;
        // Whether this uniform is a plain uniform or a member of a UBO.
        Flavor flavor;
        // If this uniform is a member of a UBO, this is the index of the UBO within the `ubos_output` list of UBOs the shader requires.
        // If this uniform is a plain uniform, this is the location of the uniform.
        uint32_t index;
        // If this uniform is a member of a UBO, this is the byte offset where this uniform is located within the UBO.
        // If this uniform is a plain uniform, this is always zero.
        size_t offset;
    };

    struct Texture
    {
        enum Type
        {
            // Unknown type.
            UNKNOWN,
            // 1-dimensional texture.
            TEXTURE_1D,
            // 2-dimensional texture.
            TEXTURE_2D,
            // 3-dimensional texture.
            TEXTURE_3D,
        };

        // The type of texture this is, e.g. 1D or 2D.
        Type type;
        // The OpenGL texture unit that this texture should be bound to before the GLSL shader is run.
        // For example, if this is 0, you should bind the texture to `GL_TEXTURE0`, and if this is 1, you should bind the texture to `GL_TEXTURE1`.
        uint32_t texture_unit;
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
     * uniforms_output: If compilation succeeds, this will be set to a mapping from the names of uniform variables used by the compiled GLSL shader to their descriptions. Otherwise this will be unchanged.
     * textures_output: If compilation succeeds, this will be set to a mapping from the names of textures used by the compiled GLSL shader to their descriptions. Otherwise this will be unchanged.
     * error_message: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing the compilation error.
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in bytes.
     * version: The 3-digit GLSL version number, such as 460 for GLSL as used in OpenGL 4.6.
     * dialect: Whether to target desktop GLSL or GLSL ES.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(
        std::string &glsl_output,
        std::vector<Ubo> &ubos_output,
        std::unordered_map<std::string, Uniform> &uniforms_output,
        std::unordered_map<std::string, Texture> &textures_output,
        std::string &error_message,
        const uint8_t *spirv_input,
        size_t spirv_input_size,
        uint16_t version,
        Dialect dialect
    );
};

#endif // SPIRV_TO_GLSL_H
