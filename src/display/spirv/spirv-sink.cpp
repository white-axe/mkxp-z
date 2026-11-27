/*
 ** spirv-sink.cpp
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

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "exception.h"
#include "gl-fun.h"
#include "spirv-sink.h"
#include "spirv/merge-uniforms.h"
#include "spirv/remap-textures.h"
#include "spirv/retarget-spirv-version.h"

#include <spirv/unified1/spirv.h>
#include <spirv_cross_c.h>

namespace {
    struct UniformPhysicalLayout
    {
        // Alignment of the in-memory representation of this uniform in bytes.
        uint32_t physical_alignment;
        // Size of the uniform's in-memory representation in bytes, including padding bytes.
        // This is guaranteed to be a multiple of the alignment.
        uint32_t physical_size;
        // If this uniform is a matrix or array of matrices,
        // this is true if the in-memory representation of this uniform is row-major, or false otherwise.
        bool is_physically_row_major;
        // If this uniform is a matrix or array of matrices,
        // this is the matrix stride (i.e. the number of bytes between adjacent rows or columns, depending on if this matrix is row-major or column-major) of this uniform in memory.
        uint32_t physical_matrix_stride;
        // If this type is an array,
        // these are the array strides of this uniform's in-memory representation, from least significant to most significant.
        std::vector<uint32_t> physical_array_strides;
    };
}

static bool supports_row_major_matrices(SpirvSink::Backend backend) {
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: return true; // All OpenGL versions that support uniform buffers also support row-major matrices. For OpenGL versions that don't support uniform buffers, we don't really care, since the `glUniform*` functions used for setting plain uniforms automatically handle the conversion for us.
    }
}

static bool supports_column_major_matrices(SpirvSink::Backend backend) {
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: return true;
    }
}

static UniformPhysicalLayout get_uniform_physical_layout(SpirvSink::Backend backend, const mkxp_spirv_utils::merge_uniforms_detail::Uniform &uniform) noexcept {
    if (uniform.type.array_dimensions.empty()) {
        if (uniform.type.columns == 1) {
            if (uniform.type.vector_size == 1) { // Scalar
                switch (uniform.type.scalar_type) {
                    case SpirvType::ScalarType::INT8:
                    case SpirvType::ScalarType::UINT8:
                        return {1, 1};
                    case SpirvType::ScalarType::INT16:
                    case SpirvType::ScalarType::UINT16:
                    case SpirvType::ScalarType::FP16:
                        return {2, 2};
                    case SpirvType::ScalarType::UNKNOWN:
                    case SpirvType::ScalarType::INT32:
                    case SpirvType::ScalarType::UINT32:
                    case SpirvType::ScalarType::FP32:
                        return {4, 4};
                    case SpirvType::ScalarType::INT64:
                    case SpirvType::ScalarType::UINT64:
                    case SpirvType::ScalarType::FP64:
                        return {8, 8};
                }
            } else { // Vector
                mkxp_spirv_utils::merge_uniforms_detail::Uniform scalar = uniform;
                scalar.type.vector_size = 1;
                UniformPhysicalLayout scalar_layout = get_uniform_physical_layout(backend, scalar);
                scalar_layout.physical_alignment *= uniform.type.vector_size == 3 ? 4 : uniform.type.vector_size; // A 3-component vector has its alignment rounded up to 4 components
                scalar_layout.physical_size = scalar_layout.physical_size;
                return scalar_layout;
            }
        } else { // Matrix
            mkxp_spirv_utils::merge_uniforms_detail::Uniform vector_array = uniform;
            vector_array.type.columns = 1;
            bool is_physically_row_major = vector_array.is_logically_row_major ? supports_row_major_matrices(backend) : !supports_column_major_matrices(backend);
            if (is_physically_row_major) {
                vector_array.type.vector_size = uniform.type.columns;
                vector_array.type.array_dimensions = {uniform.type.vector_size};
            } else {
                vector_array.type.array_dimensions = {uniform.type.columns};
            }
            UniformPhysicalLayout vector_array_layout = get_uniform_physical_layout(backend, vector_array);
            assert(vector_array_layout.physical_array_strides.size() == 1);
            vector_array_layout.is_physically_row_major = is_physically_row_major;
            vector_array_layout.physical_matrix_stride = vector_array_layout.physical_array_strides.back();
            vector_array_layout.physical_array_strides.clear();
            return vector_array_layout;
        }
    } else if (uniform.type.columns == 1) { // Array of scalars or vectors
        uint32_t array_size = 1;
        for (uint32_t dimension : uniform.type.array_dimensions) {
            array_size *= dimension;
        }
        mkxp_spirv_utils::merge_uniforms_detail::Uniform vector = uniform;
        vector.type.array_dimensions = {};
        UniformPhysicalLayout vector_layout = get_uniform_physical_layout(backend, vector);
        // Round the alignment and size of each element of the array up to the nearest multiple of 16
        vector_layout.physical_alignment = (((uint64_t)vector_layout.physical_alignment + (uint64_t)15) & ~(uint64_t)15);
        vector_layout.physical_size = (((uint64_t)vector_layout.physical_size + (uint64_t)15) & ~(uint64_t)15);
        vector_layout.physical_size *= array_size;
        assert(vector_layout.physical_array_strides.empty());
        uint32_t size = vector_layout.physical_size;
        for (uint32_t dimension : uniform.type.array_dimensions) {
            size /= dimension;
            vector_layout.physical_array_strides.push_back(size);
        }
        return vector_layout;
    } else { // Array of matrices
        uint32_t array_size = 1;
        for (uint32_t dimension : uniform.type.array_dimensions) {
            array_size *= dimension;
        }
        mkxp_spirv_utils::merge_uniforms_detail::Uniform vector_array = uniform;
        vector_array.type.columns = 1;
        bool is_physically_row_major = vector_array.is_logically_row_major ? supports_row_major_matrices(backend) : !supports_column_major_matrices(backend);
        if (is_physically_row_major) {
            vector_array.type.vector_size = uniform.type.columns;
            vector_array.type.array_dimensions = {array_size * uniform.type.vector_size};
        } else {
            vector_array.type.array_dimensions = {array_size * uniform.type.columns};
        }
        UniformPhysicalLayout vector_array_layout = get_uniform_physical_layout(backend, vector_array);
        assert(vector_array_layout.physical_array_strides.size() == 1);
        vector_array_layout.is_physically_row_major = is_physically_row_major;
        vector_array_layout.physical_matrix_stride = vector_array_layout.physical_array_strides.back();
        vector_array_layout.physical_array_strides.clear();
        return vector_array_layout;
    }
}

SpirvSink::SpirvSink(Backend backend) : context(nullptr), backend(backend), optimizer(SpirvOptimizer::Type::PERFORMANCE) {
    if (spvc_context_create((spvc_context *)&context) != SPVC_SUCCESS) {
        throw Exception(Exception::MKXPError, "Failed to initialize SPIRV-Cross context");
    }

    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: new(&glsl_backend) SpirvToGlsl(); break;
    }
}

SpirvSink::~SpirvSink() {
    if (context != nullptr) {
        spvc_context_destroy((spvc_context)context);
        switch (backend) {
            case SpirvSink::Backend::OPENGL_GLSL: glsl_backend.~SpirvToGlsl(); break;
        }
    }
}

SpirvSink::SpirvSink(SpirvSink &&other) noexcept : context(other.context), backend(other.backend), optimizer(std::move(other.optimizer)), validator(std::move(other.validator)) {
    other.context = nullptr;
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: glsl_backend = std::move(other.glsl_backend); break;
    }
}

SpirvSink &SpirvSink::operator=(SpirvSink &&other) noexcept {
    this->~SpirvSink();
    new(this) SpirvSink(std::move(other));
    return *this;
}

static size_t get_execution_model_index(const SpirvExecutionModel *execution_models, size_t execution_models_len, SpirvExecutionModel execution_model) noexcept {
    size_t execution_model_index = (size_t)-1;
    for (size_t i = 0; i < execution_models_len; ++i) {
        if (execution_models[i] == execution_model) {
            execution_model_index = i;
            break;
        }
    }
    assert(execution_model_index != (size_t)-1);
    return execution_model_index;
}

namespace {
    struct GlShaders
    {
        std::vector<GLuint> shaders;

        GlShaders() {
        }

        ~GlShaders() {
            gl.DeleteBuffers(shaders.size(), shaders.data());
        }

        GlShaders(const GlShaders &other) = delete;
        GlShaders &operator=(const GlShaders &other) = delete;
    };
}

#define SPVC_CHECK(result) do { \
    if ((result) != SPVC_SUCCESS) { \
        error_message = spvc_context_get_last_error_string(context); \
        spvc_context_release_allocations(context); \
        return false; \
    } \
} while (0)

static bool get_execution_model(
    spvc_context context,
    const uint32_t *shader,
    size_t shader_size,
    SpirvExecutionModel &execution_model_output,
    std::string &error_message
) {
    spvc_parsed_ir ir;
    SPVC_CHECK(spvc_context_parse_spirv(context, (const SpvId *)shader, shader_size, &ir));

    spvc_compiler compiler;
    SPVC_CHECK(spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    SpvExecutionModel execution_model = spvc_compiler_get_execution_model(compiler);

    spvc_context_release_allocations(context);

    switch (execution_model) {
        case SpvExecutionModelVertex: execution_model_output = SpirvExecutionModel::VERTEX; break;
        case SpvExecutionModelTessellationControl: execution_model_output = SpirvExecutionModel::TESSELLATION_CONTROL; break;
        case SpvExecutionModelTessellationEvaluation: execution_model_output = SpirvExecutionModel::TESSELLATION_EVALUATION; break;
        case SpvExecutionModelGeometry: execution_model_output = SpirvExecutionModel::GEOMETRY; break;
        case SpvExecutionModelFragment: execution_model_output = SpirvExecutionModel::FRAGMENT; break;
        default:
            error_message = "Unsupported SPIR-V execution model " + std::to_string(execution_model);
            return false;
    }
    return true;
}

static bool get_uniforms(
    spvc_context context,
    const uint32_t *shader,
    size_t shader_size,
    std::unordered_map<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform> &uniforms_output,
    std::unordered_map<std::string, uint32_t> &uniform_types_output,
    mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode &uniform_trie_output,
    std::string &error_message
) {
    spvc_parsed_ir ir;
    SPVC_CHECK(spvc_context_parse_spirv(context, (const SpvId *)shader, shader_size, &ir));

    spvc_compiler compiler;
    SPVC_CHECK(spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    spvc_resources resources;
    SPVC_CHECK(spvc_compiler_create_shader_resources(compiler, &resources));

    const spvc_reflected_resource *uniform_buffer_list;
    size_t uniform_buffer_list_size;
    SPVC_CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &uniform_buffer_list, &uniform_buffer_list_size));

    struct Node
    {
        mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode &trie_node;
        const char *name;
        spvc_type_id type_id;
        uint32_t member_size;
        bool member_is_row_major;
        uint32_t member_matrix_stride;
        uint32_t member_array_stride;
        size_t member_index;
    };
    std::vector<Node> stack;

    for (uint32_t uniform_buffer_index = 0; uniform_buffer_index < (uint32_t)uniform_buffer_list_size; ++uniform_buffer_index) {
        const char *uniform_buffer_name = spvc_compiler_get_name(compiler, uniform_buffer_list[uniform_buffer_index].id);

        uniform_trie_output.uniform_name.clear();
        stack.emplace_back(Node {
            *uniform_trie_output.children.emplace(uniform_buffer_list[uniform_buffer_index].id, std::make_unique<mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode>()).first->second,
            uniform_buffer_name,
            uniform_buffer_list[uniform_buffer_index].base_type_id,
            false,
            0,
            0,
            0,
        });

        while (!stack.empty()) {
            Node &node = stack.back();
            spvc_type type = spvc_compiler_get_type_handle(compiler, node.type_id);
            spvc_basetype basetype = spvc_type_get_basetype(type);
            if (basetype == SPVC_BASETYPE_STRUCT) {
                if (spvc_type_get_num_array_dimensions(type) != 0) {
                    error_message = "Arrays of uniform structs are unsupported";
                    spvc_context_release_allocations(context);
                    return false;
                }
                if (node.member_index < (size_t)spvc_type_get_num_member_types(type)) {
                    size_t member_index = node.member_index++;
                    size_t size;
                    if (spvc_compiler_get_declared_struct_member_size(compiler, type, member_index, &size) != SPVC_SUCCESS) {
                        error_message = "Failed to parse uniforms";
                        spvc_context_release_allocations(context);
                        return false;
                    }
                    uint32_t array_stride;
                    if (spvc_type_get_num_array_dimensions(spvc_compiler_get_type_handle(compiler, spvc_type_get_member_type(type, member_index))) == 0) {
                        array_stride = 0;
                    } else if (spvc_compiler_type_struct_member_array_stride(compiler, type, member_index, &array_stride) != SPVC_SUCCESS) {
                        error_message = "Failed to parse uniforms";
                        spvc_context_release_allocations(context);
                        return false;
                    }
                    stack.emplace_back(Node {
                        *node.trie_node.children.emplace(member_index, std::make_unique<mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode>()).first->second,
                        spvc_compiler_get_member_name(compiler, node.type_id, member_index),
                        spvc_type_get_member_type(type, member_index),
                        (uint32_t)size,
                        spvc_compiler_get_member_decoration(compiler, node.type_id, member_index, SpvDecorationRowMajor) != 0,
                        spvc_compiler_get_member_decoration(compiler, node.type_id, member_index, SpvDecorationMatrixStride),
                        array_stride,
                        0,
                    });
                } else {
                    stack.pop_back();
                }
            } else {
                SpirvType uniform_type;
                switch (basetype) {
                    case SPVC_BASETYPE_INT8: uniform_type.scalar_type = SpirvType::ScalarType::INT8; break;
                    case SPVC_BASETYPE_UINT8: uniform_type.scalar_type = SpirvType::ScalarType::UINT8; break;
                    case SPVC_BASETYPE_INT16: uniform_type.scalar_type = SpirvType::ScalarType::INT16; break;
                    case SPVC_BASETYPE_UINT16: uniform_type.scalar_type = SpirvType::ScalarType::UINT16; break;
                    case SPVC_BASETYPE_INT32: uniform_type.scalar_type = SpirvType::ScalarType::INT32; break;
                    case SPVC_BASETYPE_UINT32: uniform_type.scalar_type = SpirvType::ScalarType::UINT32; break;
                    case SPVC_BASETYPE_INT64: uniform_type.scalar_type = SpirvType::ScalarType::INT64; break;
                    case SPVC_BASETYPE_UINT64: uniform_type.scalar_type = SpirvType::ScalarType::UINT64; break;
                    case SPVC_BASETYPE_FP16: uniform_type.scalar_type = SpirvType::ScalarType::FP16; break;
                    case SPVC_BASETYPE_FP32: uniform_type.scalar_type = SpirvType::ScalarType::FP32; break;
                    case SPVC_BASETYPE_FP64: uniform_type.scalar_type = SpirvType::ScalarType::FP64; break;
                    default: uniform_type.scalar_type = SpirvType::ScalarType::UNKNOWN; break;
                }
                uniform_type.vector_size = spvc_type_get_vector_size(type);
                uniform_type.columns = spvc_type_get_columns(type);
                uniform_type.array_dimensions = std::vector<uint32_t>(spvc_type_get_num_array_dimensions(type));
                for (uint32_t array_dimension_index = 0; array_dimension_index < uniform_type.array_dimensions.size(); ++array_dimension_index) {
                    uniform_type.array_dimensions[array_dimension_index] = spvc_type_get_array_dimension(type, array_dimension_index);
                }
                mkxp_spirv_utils::merge_uniforms_detail::Uniform uniform = {std::move(uniform_type)};
                uniform.is_logically_row_major = node.member_is_row_major;
                uniform.logical_matrix_stride = node.member_matrix_stride;
                uniform.logical_array_stride = node.member_array_stride;
                std::string uniform_name;
                for (const Node &ancestor : stack) {
                    if (ancestor.name[0] != 0) {
                        uniform_name += ancestor.name;
                        if (&ancestor != &node) {
                            uniform_name += '.';
                        }
                    }
                }
                node.trie_node.uniform_name = uniform_name;
                if (uniform_name.empty()) {
                    error_message = "All uniforms must have a name";
                    spvc_context_release_allocations(context);
                    return false;
                }
                {
                    auto it = uniforms_output.find(uniform_name);
                    if (it != uniforms_output.end() && it->second != uniform) {
                        error_message = "Uniform '" + uniform_name + "' redeclared with a different type";
                        spvc_context_release_allocations(context);
                        return false;
                    }
                }
                uniforms_output.emplace(uniform_name, std::move(uniform));
                uniform_types_output.emplace(std::move(uniform_name), node.type_id);
                stack.pop_back();
            }
        }
    }

    spvc_context_release_allocations(context);
    return true;
}

static bool get_textures(
    spvc_context context,
    const uint32_t *shader,
    size_t shader_size,
    std::unordered_map<std::string, SpirvTexture::Type> &textures_output,
    std::unordered_map<std::string, uint32_t> &texture_ids_output,
    std::string &error_message
) {
    spvc_parsed_ir ir;
    SPVC_CHECK(spvc_context_parse_spirv(context, (const SpvId *)shader, shader_size, &ir));

    spvc_compiler compiler;
    SPVC_CHECK(spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    spvc_resources resources;
    SPVC_CHECK(spvc_compiler_create_shader_resources(compiler, &resources));

    const spvc_reflected_resource *texture_list;
    size_t texture_list_size;
    SPVC_CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &texture_list, &texture_list_size));

    for (uint32_t texture_index = 0; texture_index < (uint32_t)texture_list_size; ++texture_index) {
        spvc_type texture_type = spvc_compiler_get_type_handle(compiler, texture_list[texture_index].base_type_id);
        SpirvTexture::Type type;
        switch (spvc_type_get_image_dimension(texture_type)) {
            case SpvDim1D: type = SpirvTexture::Type::TEXTURE_1D; break;
            case SpvDim2D: type = SpirvTexture::Type::TEXTURE_2D; break;
            case SpvDim3D: type = SpirvTexture::Type::TEXTURE_3D; break;
            default: type = SpirvTexture::Type::UNKNOWN; break;
        }
        const char *texture_name = texture_list[texture_index].name;
        if (texture_name[0] == 0) {
            error_message = "All textures must have a name";
            spvc_context_release_allocations(context);
            return false;
        }
        {
            auto it = textures_output.find(texture_name);
            if (it != textures_output.end() && it->second != type) {
                error_message = "Texture '" + std::string(texture_name) + "' redeclared with a different type";
                spvc_context_release_allocations(context);
                return false;
            }
        }
        textures_output.emplace(texture_name, type);
        texture_ids_output.emplace(texture_name, texture_list[texture_index].id);
    }

    spvc_context_release_allocations(context);
    return true;
}

#define TRANSFORM_DETAIL(transform, spirv_input, spirv_input_size, spirv_output, error_message, ...) do { \
    std::vector<uint32_t> _spirv_output; \
    if ( \
        !(transform)( \
            (spirv_input), \
            (spirv_input_size), \
            __VA_ARGS__ \
            _spirv_output, \
            (error_message) \
        ) \
            || !validator.validate( \
                _spirv_output.data(), \
                _spirv_output.size(), \
                (error_message) \
            ) \
    ) { \
        (error_message) = #transform " at " __FILE__ " line " + std::to_string(__LINE__) + " failed: " + (error_message); \
        return false; \
    } \
    (spirv_output) = std::move(_spirv_output); \
} while (0)

#define TRANSFORM_INPUT(transform, ...) TRANSFORM_DETAIL(transform, untransformed_shader.first, untransformed_shader.second, shader, error_message, __VA_ARGS__)
#define TRANSFORM(transform, ...) TRANSFORM_DETAIL(transform, shader.data(), shader.size(), shader, error_message, __VA_ARGS__)

bool SpirvSink::compile(
    const std::pair<const uint32_t *, size_t> *shaders,
    size_t shaders_len,
    Program &program_output,
    std::string &error_message
) {
    static const SpirvExecutionModel execution_models[] = {
        SpirvExecutionModel::VERTEX,
        SpirvExecutionModel::TESSELLATION_CONTROL,
        SpirvExecutionModel::TESSELLATION_EVALUATION,
        SpirvExecutionModel::GEOMETRY,
        SpirvExecutionModel::FRAGMENT,
    };
    std::vector<std::vector<uint32_t>> shader_for_execution_model(sizeof execution_models / sizeof *execution_models);
    std::unordered_map<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform> uniform_map;
    std::vector<std::unordered_map<std::string, uint32_t>> uniform_type_map_for_execution_model(sizeof execution_models / sizeof *execution_models);
    std::vector<mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode> uniform_trie_for_execution_model(sizeof execution_models / sizeof *execution_models);
    std::unordered_map<std::string, SpirvTexture::Type> texture_map;
    std::vector<std::unordered_map<std::string, uint32_t>> texture_id_map_for_execution_model(sizeof execution_models / sizeof *execution_models);

    for (size_t shader_index = 0; shader_index < shaders_len; ++shader_index) {
        const std::pair<const uint32_t *, size_t> &untransformed_shader = shaders[shader_index];

        if (!validator.validate(
            untransformed_shader.first,
            untransformed_shader.second,
            error_message
        )) {
            return false;
        }

        SpirvExecutionModel execution_model;
        if (!get_execution_model(
            (spvc_context)context,
            untransformed_shader.first,
            untransformed_shader.second,
            execution_model,
            error_message
        )) {
            return false;
        }

        size_t execution_model_index = get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model);
        if (!shader_for_execution_model[execution_model_index].empty()) {
            error_message = "At most one shader for each SPIR-V execution model can be provided";
            return false;
        }

        std::vector<uint32_t> &shader = shader_for_execution_model[execution_model_index];

        TRANSFORM_INPUT(
            mkxp_spirv_utils::retarget_spirv_version,
            0x00010600,
            true,
            true,
        );

        if (!get_uniforms(
            (spvc_context)context,
            shader.data(),
            shader.size(),
            uniform_map,
            uniform_type_map_for_execution_model[execution_model_index],
            uniform_trie_for_execution_model[execution_model_index],
            error_message
        )) {
            return false;
        }

        if (!get_textures(
            (spvc_context)context,
            shader.data(),
            shader.size(),
            texture_map,
            texture_id_map_for_execution_model[execution_model_index],
            error_message
        )) {
            return false;
        }
    }

    if (
        shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, SpirvExecutionModel::VERTEX)].empty()
            || shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, SpirvExecutionModel::FRAGMENT)].empty()
    ) {
        error_message = "A vertex shader and a fragment shader must both be provided";
        return false;
    }

    // Sort the uniforms in non-ascending order of alignment to optimize for the size of the merged uniform buffer.
    std::vector<std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform>> uniforms;
    uniforms.reserve(uniform_map.size());
    for (auto it = uniform_map.begin(); it != uniform_map.end(); it = uniform_map.erase(it)) {
        uniforms.emplace_back(it->first, std::move(it->second));
    }
    std::sort(uniforms.begin(), uniforms.end(), [this](const std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform> &a, const std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform> &b) {
        return get_uniform_physical_layout(backend, a.second).physical_alignment > get_uniform_physical_layout(backend, b.second).physical_alignment;
    });

    // The merged textures are in an arbitrary order.
    std::vector<std::pair<std::string, SpirvTexture::Type>> textures;
    textures.reserve(texture_map.size());
    for (auto it = texture_map.begin(); it != texture_map.end(); it = texture_map.erase(it)) {
        textures.emplace_back(it->first, std::move(it->second));
    }

    GlShaders gl_shaders;

    for (size_t execution_model_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        std::vector<uint32_t> &shader = shader_for_execution_model[execution_model_index];
        if (shader.empty()) {
            continue;
        }

        TRANSFORM(
            mkxp_spirv_utils::merge_uniforms,
            uniforms,
            uniform_type_map_for_execution_model[execution_model_index],
            uniform_trie_for_execution_model[execution_model_index],
        );

        TRANSFORM(
            mkxp_spirv_utils::remap_textures,
            textures,
            texture_id_map_for_execution_model[execution_model_index],
        );

        TRANSFORM(
            optimizer.transform,
        );

        // If the underlying graphics API's maximum supported SPIR-V version is less than the one that we're using,
        // downgrade the SPIR-V version.
        uint32_t target;
        switch (backend) {
            case Backend::OPENGL_GLSL: target = glsl_backend.TARGET; break;
        }
        TRANSFORM(
            mkxp_spirv_utils::retarget_spirv_version,
            target,
            false,
            true,
        );
    }

    static std::random_device random_device;
    std::random_device::result_type cookie = random_device();

    for (size_t execution_model_index = 0, stage_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        const std::vector<uint32_t> &shader = shader_for_execution_model[execution_model_index];
        if (shader.empty()) {
            continue;
        }

        SpirvExecutionModel execution_model = execution_models[execution_model_index];

        switch (backend) {
            case Backend::OPENGL_GLSL:
                {
                    std::string glsl;
                    bool result = glsl_backend.compile(
                        shader.data(),
                        shader.size(),
                        (std::to_string(cookie) + "_stage" + std::to_string(stage_index)).c_str(),
                        (std::to_string(cookie) + "_stage" + std::to_string(stage_index + 1)).c_str(),
                        gl.glsl_info.version,
                        gl.glsl_info.es ? SpirvToGlsl::Dialect::GLSL_ES : gl.glsl_info.have_420pack ? SpirvToGlsl::Dialect::GLSL_WITH_420PACK : SpirvToGlsl::Dialect::GLSL_WITHOUT_420PACK,
                        glsl,
                        error_message
                    );
                    if (!result) {
                        return false;
                    }
                    ++stage_index;

                    GLenum gl_shader_type;
                    switch (execution_model) {
                        case SpirvExecutionModel::VERTEX: gl_shader_type = GL_VERTEX_SHADER; break;
                        case SpirvExecutionModel::TESSELLATION_CONTROL: gl_shader_type = GL_TESS_CONTROL_SHADER; break;
                        case SpirvExecutionModel::TESSELLATION_EVALUATION: gl_shader_type = GL_TESS_EVALUATION_SHADER; break;
                        case SpirvExecutionModel::GEOMETRY: gl_shader_type = GL_GEOMETRY_SHADER; break;
                        case SpirvExecutionModel::FRAGMENT: gl_shader_type = GL_FRAGMENT_SHADER; break;
                    }
                    GLuint shader_index = gl.CreateShader(gl_shader_type);
                    if (shader_index == 0) {
                        error_message = "Failed to create OpenGL shader";
                        return false;
                    }
                    gl_shaders.shaders.push_back(shader_index);

                    const char *glsl_c_str = glsl.c_str();
                    gl.ShaderSource(shader_index, 1, &glsl_c_str, nullptr);
                    gl.CompileShader(shader_index);
                    GLint gl_status;
                    gl.GetShaderiv(shader_index, GL_COMPILE_STATUS, &gl_status);
                    if (gl_status != GL_TRUE) {
                        GLint log_length;
                        gl.GetShaderiv(shader_index, GL_INFO_LOG_LENGTH, &log_length);
                        error_message.resize((size_t)log_length);
                        gl.GetShaderInfoLog(shader_index, log_length, nullptr, (GLchar *)&error_message[0]);
                        return false;
                    }
                }
                break;
        }
    }

    Program program;
    program.backend = backend;

    switch (backend) {
        case Backend::OPENGL_GLSL:
            {
                program.gl.program_index = gl.CreateProgram();
                if (program.gl.program_index == 0) {
                    error_message = "Failed to create OpenGL program";
                }

                for (GLuint shader_index : gl_shaders.shaders) {
                    gl.AttachShader(program.gl.program_index, shader_index);
                }

                gl.LinkProgram(program.gl.program_index);
                GLint gl_status;
                gl.GetProgramiv(program.gl.program_index, GL_LINK_STATUS, &gl_status);
                if (gl_status != GL_TRUE) {
                    GLint log_length;
                    gl.GetProgramiv(program.gl.program_index, GL_INFO_LOG_LENGTH, &log_length);
                    error_message.resize((size_t)log_length);
                    gl.GetProgramInfoLog(program.gl.program_index, log_length, nullptr, (GLchar *)&error_message[0]);
                    return false;
                }
            }
            break;
    }

    program_output = std::move(program);
    return true;
}

SpirvSink::Program::Program() noexcept : initialized(false) {
}

SpirvSink::Program::~Program() {
    if (!initialized) {
        return;
    }
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: gl.~Gl(); break;
    }
}

SpirvSink::Program::Program(SpirvSink::Program &&other) noexcept : initialized(other.initialized), backend(other.backend), shaders(std::move(other.shaders)) {
    other.initialized = false;
    if (!initialized) {
        return;
    }
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: new(&gl) Gl(); gl = std::move(other.gl); break;
    }
}

SpirvSink::Program &SpirvSink::Program::operator=(SpirvSink::Program &&other) noexcept {
    this->~Program();
    new(this) Program(std::move(other));
    return *this;
}
