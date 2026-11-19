/*
** spirv.h
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

#ifndef SPIRV_H
#define SPIRV_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum SpirvTarget
{
    // SPIR-V 1.0, the maximum supported SPIR-V version in Vulkan 1.0 and OpenGL
    SPIRV_1_0,
    // SPIR-V 1.1
    SPIRV_1_1,
    // SPIR-V 1.2
    SPIRV_1_2,
    // SPIR-V 1.3, the maximum supported SPIR-V version in Vulkan 1.1 if the VK_KHR_spirv_1_4 Vulkan extension is unsupported
    SPIRV_1_3,
    // SPIR-V 1.4, the maximum supported SPIR-V version in Vulkan 1.1 if the VK_KHR_spirv_1_4 Vulkan extension is supported
    SPIRV_1_4,
    // SPIR-V 1.5, the maximum supported SPIR-V version in Vulkan 1.2
    SPIRV_1_5,
    // SPIR-V 1.6, the maximum supported SPIR-V version in Vulkan 1.3 and Vulkan 1.4
    SPIRV_1_6,
};

enum SpirvExecutionModel
{
    // Vertex shader.
    VERTEX,
    // Tessellation control shader. In Direct3D, this is known as a hull shader.
    TESSELLATION_CONTROL,
    // Tessellation evaluation shader. In Direct3D, this is known as a domain shader.
    TESSELLATION_EVALUATION,
    // Geometry shader.
    GEOMETRY,
    // Fragment shader. In Direct3D, this is known as a pixel shader.
    FRAGMENT,
};

struct SpirvUniformBuffer
{
    // The size of this uniform buffer in bytes.
    size_t size;
    // The binding point that this uniform buffer should be bound to before the shader is run.
    // If binding points are not supported, this will always be zero.
    uint32_t binding;
};

struct SpirvType
{
    enum ScalarType
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

    // The scalar type of this type.
    ScalarType scalar_type;
    // `vector_size` and `columns` can each either be 1, 2, 3 or 4.
    // If both `vector_size` and `columns` are equal to 1, then this is a scalar type.
    // If `vector_size` is greater than 1 and `columns` is equal to 1, then this is a vector of `type` with `vector_size` elements.
    // If `columns` is greater than 1, then this is a matrix of `type` with `vector_size` rows and `columns` columns.
    uint8_t vector_size, columns;
    // True if this is a row-major matrix, false if this is not a matrix or is a column-major matrix.
    bool is_row_major;
    // If non-empty, this is an array, possibly multidimensional, with these dimensions.
    // It's possible for a type to be both an array type and a matrix or vector type at the same time, in which case the type is an array of matrices/vectors, not a matrix/vector of arrays.
    std::vector<uint32_t> array_dimensions;
};

struct SpirvUniform
{
    enum Flavor
    {
        // This uniform is a member of a uniform buffer.
        UNIFORM_BUFFER,
        // This uniform is an OpenGL-style plain uniform, not part of a uniform buffer.
        PLAIN,
    };

    // The type of this uniform.
    SpirvType type;
    // Whether this uniform is a plain uniform or a member of a uniform buffer.
    Flavor flavor;
    // If this uniform is a member of a uniform buffer, this is the index of the uniform buffer within the `ubos_output` list of uniform buffers the shader requires.
    // If this uniform is a plain uniform, this is the location of the uniform.
    uint32_t index;
    // If this uniform is a member of a uniform buffer, this is the byte offset where this uniform is located within the uniform buffer.
    // If this uniform is a plain uniform, this is always zero.
    size_t offset;
};

struct SpirvTexture
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
    // The texture unit that this texture should be bound to before the shader is run.
    // For example, in OpenGL, if this is 0, you should bind the texture to `GL_TEXTURE0`, and if this is 1, you should bind the texture to `GL_TEXTURE1`.
    uint32_t texture_unit;
};

struct SpirvShaderInfo
{
    std::vector<SpirvUniformBuffer> uniform_buffers;
    std::unordered_map<std::string, SpirvUniform> uniforms;
    std::unordered_map<std::string, SpirvTexture> textures;
};

#endif // SPIRV_H
