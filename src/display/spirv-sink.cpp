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

#include <spirv_cross_c.h>
#include <spirv-tools/libspirv.h>

static SpirvTarget get_target(SpirvSink::Backend backend) noexcept {
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: return SpirvToGlsl::TARGET;
    }
}

static spv_target_env get_target_env(SpirvTarget target) {
    switch (target) {
        case SpirvTarget::SPIRV_1_0: return SPV_ENV_UNIVERSAL_1_0;
        case SpirvTarget::SPIRV_1_1: return SPV_ENV_UNIVERSAL_1_1;
        case SpirvTarget::SPIRV_1_2: return SPV_ENV_UNIVERSAL_1_2;
        case SpirvTarget::SPIRV_1_3: return SPV_ENV_UNIVERSAL_1_3;
        case SpirvTarget::SPIRV_1_4: return SPV_ENV_UNIVERSAL_1_4;
        case SpirvTarget::SPIRV_1_5: return SPV_ENV_UNIVERSAL_1_5;
        case SpirvTarget::SPIRV_1_6: return SPV_ENV_UNIVERSAL_1_6;
    }
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

// Returns whether or not the SPIR-V to native shading language compiler considers SPIR-V matrices
// that aren't explicitly annotated with DecorationRowMajor or DecorationColMajor to be row-major.
static bool matrices_are_row_major_by_default(SpirvSink::Backend backend) {
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: return false;
    }
}

struct TypeLayout
{
    // Alignment of the type in bytes.
    // This is guaranteed to be a power of 2.
    uint32_t alignment;
    // Size of the type in bytes, including padding bytes.
    // This is guaranteed to be a multiple of the alignment of the type.
    uint32_t size;
    // If this type is a matrix or array of matrices, this is the matrix stride
    // (i.e. the number of bytes between adjacent rows or columns, depending on how the matrix is arranged in memory).
    // Otherwise, this is the same as the size of the type.
    // The size of the type is guaranteed to be a multiple of this matrix stride.
    uint32_t matrix_stride;
    // If this type is an array, this is the array stride for each dimension
    // (i.e. the number of bytes between adjacent subarrays).
    // The size of the type is guaranteed to be a multiple of each array stride.
    std::vector<uint32_t> array_strides;
};

static TypeLayout get_type_layout(SpirvSink::Backend backend, const SpirvType &type) noexcept {
    // If the underlying graphics API only supports one of the two possible matrix memory orderings,
    // we have to use that supported ordering when computing the size and alignment.
    // We still have to annotate the matrix as using the actual requested memory ordering even if it's unsupported,
    // since the SPIR-V to native shading language compiler needs this information to correctly transpose matrix operations for unsupported matrix memory orderings.
    bool is_row_major_in_memory;
    if (type.columns == 1) {
        is_row_major_in_memory = false;
    } else if (type.is_row_major) {
        is_row_major_in_memory = supports_row_major_matrices(backend);
    } else {
        is_row_major_in_memory = !supports_column_major_matrices(backend);
    }

    if (type.array_dimensions.empty()) {
        if (type.columns == 1) {
            if (type.vector_size == 1) { // Scalar
                switch (type.scalar_type) {
                    case SpirvType::ScalarType::INT8:
                    case SpirvType::ScalarType::UINT8:
                        return {1, 1, 1, std::vector<uint32_t>()};
                    case SpirvType::ScalarType::INT16:
                    case SpirvType::ScalarType::UINT16:
                    case SpirvType::ScalarType::FP16:
                        return {2, 2, 2, std::vector<uint32_t>()};
                    case SpirvType::ScalarType::UNKNOWN:
                    case SpirvType::ScalarType::INT32:
                    case SpirvType::ScalarType::UINT32:
                    case SpirvType::ScalarType::FP32:
                        return {4, 4, 4, std::vector<uint32_t>()};
                    case SpirvType::ScalarType::INT64:
                    case SpirvType::ScalarType::UINT64:
                    case SpirvType::ScalarType::FP64:
                        return {8, 8, 8, std::vector<uint32_t>()};
                }
            } else { // Vector
                SpirvType scalar_type = type;
                scalar_type.vector_size = 1;
                TypeLayout scalar_layout = get_type_layout(backend, scalar_type);
                scalar_layout.alignment *= type.vector_size == 3 ? 4 : type.vector_size; // A 3-component vector has its alignment rounded up to 4 components
                scalar_layout.size = scalar_layout.alignment;
                return scalar_layout;
            }
        } else { // Matrix
            SpirvType vector_array_type = type;
            vector_array_type.columns = 1;
            if (is_row_major_in_memory) {
                vector_array_type.vector_size = type.columns;
                vector_array_type.array_dimensions = {type.vector_size};
            } else {
                vector_array_type.array_dimensions = {type.columns};
            }
            TypeLayout vector_array_layout = get_type_layout(backend, vector_array_type);
            assert(vector_array_layout.array_strides.size() == 1);
            vector_array_layout.matrix_stride = vector_array_layout.array_strides.front();
            vector_array_layout.array_strides.clear();
            return vector_array_layout;
        }
    } else if (type.columns == 1) { // Array of scalars or vectors
        uint32_t array_size = 1;
        for (uint32_t dimension : type.array_dimensions) {
            array_size *= dimension;
        }
        SpirvType vector_type = type;
        vector_type.array_dimensions = {};
        TypeLayout vector_layout = get_type_layout(backend, vector_type);
        // Round the alignment and size of each element of the array up to the nearest multiple of 16
        vector_layout.alignment = (((uint64_t)vector_layout.alignment + (uint64_t)15) & ~(uint64_t)15);
        vector_layout.size = (((uint64_t)vector_layout.size + (uint64_t)15) & ~(uint64_t)15);
        vector_layout.size *= array_size;
        assert(vector_layout.array_strides.empty());
        uint32_t size = vector_layout.size;
        for (uint32_t dimension : type.array_dimensions) {
            size /= dimension;
            vector_layout.array_strides.push_back(size);
        }
        return vector_layout;
    } else { // Array of matrices
        uint32_t array_size = 1;
        for (uint32_t dimension : type.array_dimensions) {
            array_size *= dimension;
        }
        SpirvType vector_array_type = type;
        vector_array_type.columns = 1;
        if (is_row_major_in_memory) {
            vector_array_type.vector_size = type.columns;
            vector_array_type.array_dimensions = {array_size * type.vector_size};
        } else {
            vector_array_type.array_dimensions = {array_size * type.columns};
        }
        TypeLayout vector_array_layout = get_type_layout(backend, vector_array_type);
        assert(vector_array_layout.array_strides.size() == 1);
        vector_array_layout.matrix_stride = vector_array_layout.array_strides.front();
        vector_array_layout.array_strides.clear();
        return vector_array_layout;
    }
}

SpirvSink::SpirvSink(Backend backend) : spirv_cross_context(nullptr), spirv_tools_context(nullptr), backend(backend), optimizer(get_target(backend)) {
    if (spvc_context_create((spvc_context *)&spirv_cross_context) != SPVC_SUCCESS) {
        throw Exception(Exception::MKXPError, "Failed to initialize SPIRV-Cross context");
    }

    if ((spirv_tools_context = spvContextCreate(get_target_env(get_target(backend)))) == nullptr) {
        spvc_context_destroy((spvc_context)spirv_cross_context);
        spirv_cross_context = nullptr;
        throw Exception(Exception::MKXPError, "Failed to initialize SPIR-V Tools context");
    }

    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: new(&glsl_backend) SpirvToGlsl(); break;
    }
}

SpirvSink::~SpirvSink() {
    if (spirv_tools_context != nullptr) {
        spvContextDestroy((spv_context)spirv_tools_context);
    }

    if (spirv_cross_context != nullptr) {
        spvc_context_destroy((spvc_context)spirv_cross_context);
        switch (backend) {
            case SpirvSink::Backend::OPENGL_GLSL: glsl_backend.~SpirvToGlsl(); break;
        }
    }
}

SpirvSink::SpirvSink(SpirvSink &&other) noexcept : spirv_cross_context(other.spirv_cross_context), spirv_tools_context(other.spirv_tools_context), backend(other.backend), optimizer(std::move(other.optimizer)) {
    other.spirv_cross_context = nullptr;
    other.spirv_tools_context = nullptr;
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: glsl_backend = std::move(other.glsl_backend); break;
    }
}

SpirvSink &SpirvSink::operator=(SpirvSink &&other) noexcept {
    this->~SpirvSink();
    new(this) SpirvSink(std::move(other));
    return *this;
}

SpirvTarget SpirvSink::target() const noexcept {
    return get_target(backend);
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

struct UniformTrieNode
{
    // Maps an OpAccessChain index to the next node.
    std::unordered_map<uint32_t, std::unique_ptr<UniformTrieNode>> children;
    // If `children` is empty, this is the name of the uniform that this node refers to.
    // Otherwise, this is empty.
    std::string uniform_name;
};

static bool get_uniforms(
    SpirvSink::Backend backend,
    spvc_context context,
    const uint32_t *shader,
    size_t shader_size,
    std::unordered_map<std::string, SpirvType> &uniforms_output,
    UniformTrieNode &uniform_trie_output,
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

    static const char *const default_uniform_buffer_names[] = {
        "$Global", // Default name of the default uniform buffer used by glslang's HLSL frontend
        "gl_DefaultUniformBlock", // Default name of the default uniform buffer used by glslang's GLSL frontend
    };
    bool default_uniform_buffer_seen = false;
    struct Node
    {
        UniformTrieNode &trie_node;
        const char *name;
        spvc_type_id type_id;
        size_t member_index;
    };
    std::vector<Node> stack;

    for (uint32_t uniform_buffer_index = 0; uniform_buffer_index < (uint32_t)uniform_buffer_list_size; ++uniform_buffer_index) {
        const char *uniform_buffer_name = spvc_compiler_get_name(compiler, uniform_buffer_list[uniform_buffer_index].base_type_id);
        if (!default_uniform_buffer_seen) {
            for (size_t j = 0; j < sizeof default_uniform_buffer_names / sizeof *default_uniform_buffer_names; ++j) {
                if (std::strcmp(uniform_buffer_name, default_uniform_buffer_names[j]) == 0) {
                    default_uniform_buffer_seen = true;
                    uniform_buffer_name = nullptr;
                    break;
                }
            }
        }

        uniform_trie_output.uniform_name.clear();
        stack.emplace_back(Node {
            *uniform_trie_output.children.emplace(uniform_buffer_list[uniform_buffer_index].id, std::make_unique<UniformTrieNode>()).first->second,
            uniform_buffer_name,
            uniform_buffer_list[uniform_buffer_index].base_type_id,
            0,
        });

        while (!stack.empty()) {
            Node &node = stack.back();
            spvc_type type = spvc_compiler_get_type_handle(compiler, node.type_id);
            spvc_basetype basetype = spvc_type_get_basetype(type);
            if (basetype == SPVC_BASETYPE_STRUCT) {
                if (spvc_type_get_num_array_dimensions(type) != 0) {
                    error_message = "Arrays of uniform structs are unsupported";
                    return false;
                }
                if (node.member_index < (size_t)spvc_type_get_num_member_types(type)) {
                    size_t member_index = node.member_index++;
                    stack.emplace_back(Node {
                        *node.trie_node.children.emplace(member_index, std::make_unique<UniformTrieNode>()).first->second,
                        spvc_compiler_get_member_name(compiler, node.type_id, member_index),
                        spvc_type_get_member_type(type, member_index),
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
                uniform_type.is_row_major = uniform_type.columns != 1 && (matrices_are_row_major_by_default(backend) ? spvc_compiler_get_decoration(compiler, node.type_id, SpvDecorationColMajor) == 0 : spvc_compiler_get_decoration(compiler, node.type_id, SpvDecorationRowMajor) != 0);
                uniform_type.array_dimensions = std::vector<uint32_t>(spvc_type_get_num_array_dimensions(type));
                for (uint32_t array_dimension_index = 0; array_dimension_index < uniform_type.array_dimensions.size(); ++array_dimension_index) {
                    uniform_type.array_dimensions[array_dimension_index] = spvc_type_get_array_dimension(type, array_dimension_index);
                }
                std::string uniform_name;
                for (const Node &ancestor : stack) {
                    if (ancestor.name != nullptr) {
                        uniform_name += ancestor.name;
                        if (&ancestor != &node) {
                            uniform_name += '.';
                        }
                    }
                }
                node.trie_node.uniform_name = uniform_name;
                uniforms_output.emplace(std::move(uniform_name), std::move(uniform_type));
                stack.pop_back();
            }
        }
    }

    spvc_context_release_allocations(context);
    return true;
}

static bool allocate_id(uint32_t &id_bound, uint32_t &output) noexcept {
    return (output = id_bound++) != UINT32_MAX;
}

static void push_instruction(std::vector<uint32_t> &output, SpvOp opcode, uint16_t num_words) {
    output.push_back((uint32_t)opcode | ((uint32_t)num_words << 16));
}

static void push_string_literal(std::vector<uint32_t> &output, const char *string_literal) {
    // SPIR-V string literals are encoded by splitting a null-terminated UTF-8 string, including the null terminator, into chunks of 4 bytes,
    // padding the string with up to 3 additional null bytes at the end to make its length a multiple of 4 bytes if needed.
    // Then the bytes are encoded as 32-bit words in little-endian byte order:
    // the first byte in each chunk goes in the least-significant byte of the 32-bit word,
    // and the last byte in each chunk goes in the most-significant byte.
    // It turns out that just using memcpy from the string into the words works on both big-endian and little-endian machines.
    size_t size = std::strlen(string_literal) + 1;
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word;
        std::strncpy((char *)&word, string_literal + i, 4);
        output.push_back(word);
    }
}

static uint16_t num_string_literal_words(const char *string_literal) noexcept {
    size_t size = std::strlen(string_literal) + 1;
    return size / 4 + (size % 4 == 0 ? 0 : 1);
}

static void push_type_impl(std::vector<uint32_t> &output, const SpirvType &type, std::vector<uint32_t> &type_ids) {
    if (type.array_dimensions.empty()) {
        uint32_t type_id = type_ids.back();
        type_ids.pop_back();
        if (type.columns == 1) {
            if (type.vector_size == 1) { // Scalar
                switch (type.scalar_type) {
                    case SpirvType::ScalarType::INT8:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(8); // width
                        output.push_back(1); // signedness
                        break;
                    case SpirvType::ScalarType::UINT8:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(8); // width
                        output.push_back(0); // signedness
                        break;
                    case SpirvType::ScalarType::INT16:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(16); // width
                        output.push_back(1); // signedness
                        break;
                    case SpirvType::ScalarType::UINT16:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(16); // width
                        output.push_back(0); // signedness
                        break;
                    case SpirvType::ScalarType::INT32:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(32); // width
                        output.push_back(1); // signedness
                        break;
                    case SpirvType::ScalarType::UNKNOWN:
                    case SpirvType::ScalarType::UINT32:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(32); // width
                        output.push_back(0); // signedness
                        break;
                    case SpirvType::ScalarType::INT64:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(64); // width
                        output.push_back(1); // signedness
                        break;
                    case SpirvType::ScalarType::UINT64:
                        push_instruction(output, SpvOpTypeInt, 4);
                        output.push_back(type_id);
                        output.push_back(64); // width
                        output.push_back(0); // signedness
                        break;
                    case SpirvType::ScalarType::FP16:
                        push_instruction(output, SpvOpTypeFloat, 3);
                        output.push_back(type_id);
                        output.push_back(16);
                        break;
                    case SpirvType::ScalarType::FP32:
                        push_instruction(output, SpvOpTypeFloat, 3);
                        output.push_back(type_id);
                        output.push_back(32);
                        break;
                    case SpirvType::ScalarType::FP64:
                        push_instruction(output, SpvOpTypeFloat, 3);
                        output.push_back(type_id);
                        output.push_back(64);
                        break;
                }
            } else { // Vector
                uint32_t scalar_type_id = type_ids.back();
                SpirvType scalar_type = type;
                scalar_type.vector_size = 1;
                push_type_impl(output, scalar_type, type_ids);
                push_instruction(output, SpvOpTypeVector, 4);
                output.push_back(type_id);
                output.push_back(scalar_type_id);
                output.push_back(type.vector_size);
            }
        } else { // Matrix
            uint32_t vector_type_id = type_ids.back();
            SpirvType vector_type = type;
            vector_type.columns = 1;
            push_type_impl(output, vector_type, type_ids);
            push_instruction(output, SpvOpTypeMatrix, 4);
            output.push_back(type_id);
            output.push_back(vector_type_id);
            output.push_back(type.columns);
        }
    } else { // Array
        std::vector<uint32_t> array_type_ids(type_ids.begin() + (type_ids.size() - type.array_dimensions.size()), type_ids.end());
        type_ids.resize(type_ids.size() - type.array_dimensions.size());
        uint32_t element_type_id = type_ids.back();
        SpirvType element_type = type;
        element_type.array_dimensions.clear();
        push_type_impl(output, element_type, type_ids);
        for (size_t dimension_index = 0; dimension_index < type.array_dimensions.size(); ++dimension_index) {
            push_instruction(output, SpvOpTypeArray, 4);
            output.push_back(array_type_ids[array_type_ids.size() - type.array_dimensions.size() + dimension_index]);
            output.push_back(dimension_index == 0 ? element_type_id : array_type_ids[array_type_ids.size() - type.array_dimensions.size() + dimension_index - 1]);
            output.push_back(type.array_dimensions[type.array_dimensions.size() - dimension_index - 1]);
        }
    }
}

static void push_type(std::vector<uint32_t> &output, const SpirvType &type, std::vector<uint32_t> type_ids) {
    return push_type_impl(output, type, type_ids);
}

static bool rewrite_shader_uniforms(
    SpirvSink::Backend backend,
    spv_context context,
    const uint32_t *shader,
    size_t shader_size,
    const std::vector<std::pair<std::string, SpirvType>> &uniforms,
    const UniformTrieNode &uniform_trie,
    std::vector<uint32_t> &shader_output,
    std::string &error_message
) {
    std::unordered_map<std::string, uint32_t> uniform_index_map;
    for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
        uniform_index_map.emplace(uniforms[uniform_index].first, uniform_index);
    }

    struct Data
    {
        // SPIR-V version, encoded in the same way as it's supposed to appear in a SPIR-V module.
        uint32_t version;
        // SPIR-V ID for a signed 32-bit integer type that we will use to define the constants used to access uniforms within our merged uniform buffer.
        uint32_t int32_type_id;
        // SPIR-V ID for our merged uniform buffer.
        uint32_t merged_uniform_buffer_id;
        // SPIR-V ID for the struct type for our merged uniform buffer.
        uint32_t merged_uniform_buffer_type_id;
        // SPIR-V ID for a pointer type that points to the struct type for our merged uniform buffer.
        uint32_t merged_uniform_buffer_pointer_id;
        // For each uniform in our merged uniform buffer, this contains a SPIR-V ID for a constant used to access the corresponding uniform.
        std::vector<uint32_t> merged_uniform_buffer_member_index_ids;
        // For each uniform in our merged uniform buffer, this contains one or more SPIR-V IDs for the type of that uniform.
        // For scalars, there is one ID.
        // For vectors, there are two IDs: one for the scalar type of the uniform, followed by one for the uniform itself.
        // For matrices, there are three IDs: one for the scalar type of the uniform, followed by one for the vector type of the columns of the uniform, followed by one for the uniform itself.
        // For arrays, there are the IDs for the elements of the array, followed by a number of IDs equal to the number of dimensions of the array, representing the array types from least significant to most significant (i.e. the last of these IDs is the ID for the uniform itself).
        std::vector<std::vector<uint32_t>> merged_uniform_buffer_member_type_ids;
        // A map from the SPIR-V IDs of the integer constants defined in the SPIR-V module to the values of those constants.
        std::unordered_map<uint32_t, uint32_t> constants;
        // A map from the SPIR-V IDs for pointer values that refer to some section of a uniform buffer to the uniform trie node for that pointer value.
        std::unordered_map<uint32_t, const UniformTrieNode &> pointers;
        // True if we are in or past the debug section of the SPIR-V module, otherwise false.
        bool seen_debug_section;
        // True if we are in or past the annotations section of the SPIR-V module, otherwise false.
        bool seen_annotation_section;
        // True if we are in or past the types section of the SPIR-V module, otherwise false.
        bool seen_types_section;
        SpirvSink::Backend backend;
        const std::vector<std::pair<std::string, SpirvType>> &uniforms;
        const std::unordered_map<std::string, uint32_t> &uniform_index_map;
        const UniformTrieNode &uniform_trie;
        std::vector<uint32_t> &shader_output;
        std::string error_message;
    } user_data = {
        0,
        0,
        0,
        0,
        0,
        std::vector<uint32_t>(uniforms.size()),
        std::vector<std::vector<uint32_t>>(uniforms.size()),
        std::unordered_map<uint32_t, uint32_t>(),
        std::unordered_map<uint32_t, const UniformTrieNode &>(),
        false,
        false,
        false,
        backend,
        uniforms,
        uniform_index_map,
        uniform_trie,
        shader_output,
        std::string(),
    };

    spv_diagnostic diagnostic;
    spv_result_t result = spvBinaryParse(
        context,
        &user_data,
        shader,
        shader_size,
        [](void *_user_data, spv_endianness_t endian, uint32_t magic, uint32_t version, uint32_t generator, uint32_t id_bound, uint32_t reserved) {
            Data &user_data = *(Data *)_user_data;

            // The newest SPIR-V version that we support right now is SPIR-V 1.6.
            // Newer SPIR-V versions, if any are released, may break our parser code,
            // so we need to explicitly check the version.
            if (version > 0x00010600) {
                user_data.error_message = "Unsupported SPIR-V version";
                return SPV_ERROR_WRONG_VERSION;
            }

            user_data.version = version;

            // Allocate new IDs to use for the merged uniform buffer.
            // SPIR-V Tools's optimizer should already have compacted the existing IDs inside of the shader,
            // so we should have plenty of IDs to use.
            {
                bool success = true;
                success &= allocate_id(id_bound, user_data.int32_type_id);
                success &= allocate_id(id_bound, user_data.merged_uniform_buffer_id);
                success &= allocate_id(id_bound, user_data.merged_uniform_buffer_type_id);
                success &= allocate_id(id_bound, user_data.merged_uniform_buffer_pointer_id);
                for (uint32_t &index_id : user_data.merged_uniform_buffer_member_index_ids) {
                    success &= allocate_id(id_bound, index_id);
                }
                for (size_t i = 0; i < user_data.uniforms.size(); ++i) {
                    const SpirvType &type = user_data.uniforms[i].second;
                    uint32_t num_type_ids = 1;
                    if (type.vector_size != 1) {
                        ++num_type_ids;
                    }
                    if (type.columns != 1) {
                        ++num_type_ids;
                    }
                    num_type_ids += type.array_dimensions.size();
                    std::vector<uint32_t> &type_ids = user_data.merged_uniform_buffer_member_type_ids[i];
                    type_ids = std::vector<uint32_t>(num_type_ids);
                    for (uint32_t &type_id : type_ids) {
                        success &= allocate_id(id_bound, type_id);
                    }
                }
                if (!success) {
                    user_data.error_message = "Failed to allocate IDs in the SPIR-V module";
                    return SPV_ERROR_INVALID_DATA;
                }
            }

            user_data.shader_output.push_back(magic);
            user_data.shader_output.push_back(version);
            user_data.shader_output.push_back(generator);
            user_data.shader_output.push_back(id_bound);
            user_data.shader_output.push_back(reserved);
            return SPV_SUCCESS;
        },
        [](void *_user_data, const spv_parsed_instruction_t *parsed_instruction) {
            Data &user_data = *(Data *)_user_data;

            if (parsed_instruction->opcode == SpvOpConstant) {
                user_data.constants.emplace(parsed_instruction->words[2], parsed_instruction->words[3]);
            }

            bool is_start_of_debug_section = false;
            bool is_start_of_annotation_section = false;
            bool is_start_of_types_section = false;

            // Note: it's intentional that there isn't a break statement at the ends of the cases.
            // We want to make sure that, for example, if we see an annotation instruction without ever having seen a debug instruction,
            // then `is_start_of_debug_section` and `is_start_of_annotation_section` must both be set to `true`.
            switch (parsed_instruction->opcode) {
                // Type declarations
                case SpvOpTypeVoid:
                case SpvOpTypeBool:
                case SpvOpTypeInt:
                case SpvOpTypeFloat:
                case SpvOpTypeVector:
                case SpvOpTypeMatrix:
                case SpvOpTypeImage:
                case SpvOpTypeSampler:
                case SpvOpTypeSampledImage:
                case SpvOpTypeArray:
                case SpvOpTypeRuntimeArray:
                case SpvOpTypeStruct:
                case SpvOpTypeOpaque:
                case SpvOpTypePointer:
                case SpvOpTypeFunction:
                case SpvOpTypeEvent:
                case SpvOpTypeDeviceEvent:
                case SpvOpTypeReserveId:
                case SpvOpTypeQueue:
                case SpvOpTypePipe:
                case SpvOpTypeForwardPointer:
                // Constant instructions (i.e. all instructions whose names start with OpConstant or OpSpecConstant)
                case SpvOpConstant:
                case SpvOpConstantComposite:
                case SpvOpConstantSampler:
                case SpvOpConstantNull:
                case SpvOpSpecConstantTrue:
                case SpvOpSpecConstantFalse:
                case SpvOpSpecConstant:
                case SpvOpSpecConstantComposite:
                case SpvOpSpecConstantOp:
                // Global variable declarations
                case SpvOpVariable:
                // The specific four instructions that are mentioned as also being allowed in this section
                case SpvOpUndef:
                case SpvOpLine:
                case SpvOpNoLine:
                case SpvOpExtInst:
                    if (!user_data.seen_types_section) {
                        user_data.seen_types_section = true;
                        is_start_of_types_section = true;
                    }

                case SpvOpDecorate:
                case SpvOpMemberDecorate:
                case SpvOpDecorationGroup:
                case SpvOpGroupDecorate:
                case SpvOpGroupMemberDecorate:
                case SpvOpDecorateId:
                case SpvOpMemberDecorateIdEXT:
                case SpvOpDecorateString:
                case SpvOpMemberDecorateString:
                    if (!user_data.seen_annotation_section) {
                        user_data.seen_annotation_section = true;
                        is_start_of_annotation_section = true;
                    }

                case SpvOpString:
                case SpvOpSourceExtension:
                case SpvOpSource:
                case SpvOpSourceContinued:
                case SpvOpName:
                case SpvOpMemberName:
                case SpvOpModuleProcessed:
                    if (!user_data.seen_debug_section) {
                        user_data.seen_debug_section = true;
                        is_start_of_debug_section = true;
                    }

                default:
                    break;
            }

            if (is_start_of_debug_section) {
                // Declare the name of our merged uniform buffer
                push_instruction(user_data.shader_output, SpvOpName, num_string_literal_words("mkxpz_spirv_sink_uniforms") + 2); // Don't have to worry about name collisions; the SPIR-V to native shading language compiler will deal with it automatically
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_id);
                push_string_literal(user_data.shader_output, "mkxpz_spirv_sink_uniforms");

                // Declare the name of our merged uniform buffer's type
                push_instruction(user_data.shader_output, SpvOpName, num_string_literal_words("mkxpz_spirv_sink_uniforms_t") + 2);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                push_string_literal(user_data.shader_output, "mkxpz_spirv_sink_uniforms_t");

                // Declare the names of our merged uniform buffer's fields
                for (size_t i = 0; i < user_data.uniforms.size(); ++i) {
                    push_instruction(user_data.shader_output, SpvOpMemberName, num_string_literal_words(user_data.uniforms[i].first.c_str()) + 3);
                    user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                    user_data.shader_output.push_back(i);
                    push_string_literal(user_data.shader_output, user_data.uniforms[i].first.c_str());
                }
            }

            if (is_start_of_annotation_section) {
                // Declare that our merged uniform buffer's type is an interface block
                push_instruction(user_data.shader_output, SpvOpDecorate, 3);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                user_data.shader_output.push_back(SpvDecorationBlock);

                // Declare the linkage of our merged uniform buffer
                push_instruction(user_data.shader_output, SpvOpDecorate, 4);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_id);
                user_data.shader_output.push_back(SpvDecorationBinding);
                user_data.shader_output.push_back(0);
                push_instruction(user_data.shader_output, SpvOpDecorate, 4);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_id);
                user_data.shader_output.push_back(SpvDecorationDescriptorSet);
                user_data.shader_output.push_back(0);

                // Declare the offsets for our merged uniform buffer's fields
                uint32_t offset = 0;
                for (size_t uniform_index = 0; uniform_index < user_data.uniforms.size(); ++uniform_index) {
                    const SpirvType &type = user_data.uniforms[uniform_index].second;
                    TypeLayout layout = get_type_layout(user_data.backend, type);

                    // Round the offset up to the nearest multiple of the alignment
                    if (offset % layout.alignment != 0) {
                        offset += layout.alignment - offset % layout.alignment;
                    }

                    push_instruction(user_data.shader_output, SpvOpMemberDecorate, 5);
                    user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                    user_data.shader_output.push_back(uniform_index);
                    user_data.shader_output.push_back(SpvDecorationOffset);
                    user_data.shader_output.push_back(offset);

                    // If it's a matrix or array of matrices, also declare whether it's row-major or column-major and the matrix stride
                    if (type.columns != 1) {
                        push_instruction(user_data.shader_output, SpvOpMemberDecorate, 4);
                        user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                        user_data.shader_output.push_back(uniform_index);
                        user_data.shader_output.push_back(type.is_row_major ? SpvDecorationRowMajor : SpvDecorationColMajor);

                        push_instruction(user_data.shader_output, SpvOpMemberDecorate, 5);
                        user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                        user_data.shader_output.push_back(uniform_index);
                        user_data.shader_output.push_back(SpvDecorationMatrixStride);
                        user_data.shader_output.push_back(layout.matrix_stride);
                    }

                    // If it's an array, also declare the array strides
                    if (!type.array_dimensions.empty()) {
                        const std::vector<uint32_t> &type_ids = user_data.merged_uniform_buffer_member_type_ids[uniform_index];
                        for (size_t dimension_index = 0; dimension_index < type.array_dimensions.size(); ++dimension_index) {
                            push_instruction(user_data.shader_output, SpvOpDecorate, 4);
                            user_data.shader_output.push_back(type_ids[type_ids.size() - dimension_index - 1]);
                            user_data.shader_output.push_back(SpvDecorationArrayStride);
                            user_data.shader_output.push_back(layout.array_strides[dimension_index]);
                        }
                    }

                    // Increase the offset by the size of this type
                    offset += layout.size;
                }
            }

            if (is_start_of_types_section) {
                // Declare a type for the constants for each member of our merged uniform buffer
                {
                    SpirvType type;
                    type.scalar_type = SpirvType::ScalarType::INT32;
                    type.vector_size = 1;
                    type.columns = 1;
                    type.is_row_major = false;
                    push_type(user_data.shader_output, type, {user_data.int32_type_id});
                }

                // Declare a constant for each member of our merged uniform buffer
                for (size_t uniform_index = 0; uniform_index < user_data.uniforms.size(); ++uniform_index) {
                    push_instruction(user_data.shader_output, SpvOpConstant, 4);
                    user_data.shader_output.push_back(user_data.int32_type_id);
                    user_data.shader_output.push_back(user_data.merged_uniform_buffer_member_index_ids[uniform_index]);
                    user_data.shader_output.push_back(uniform_index);
                }

                // Declare a type for each member of our merged uniform buffer
                for (size_t i = 0; i < user_data.uniforms.size(); ++i) {
                    push_type(user_data.shader_output, user_data.uniforms[i].second, user_data.merged_uniform_buffer_member_type_ids[i]);
                }

                // Declare a struct type for our merged uniform buffer
                push_instruction(user_data.shader_output, SpvOpTypeStruct, (uint16_t)user_data.uniforms.size() + 2);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);
                for (const std::vector<uint32_t> &type_ids : user_data.merged_uniform_buffer_member_type_ids) {
                    user_data.shader_output.push_back(type_ids.back());
                }

                // Declare a pointer type for our merged uniform buffer
                push_instruction(user_data.shader_output, SpvOpTypePointer, 4);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_pointer_id);
                user_data.shader_output.push_back(SpvStorageClassUniform);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_type_id);

                // Define the merged uniform buffer
                push_instruction(user_data.shader_output, SpvOpVariable, 4);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_pointer_id);
                user_data.shader_output.push_back(user_data.merged_uniform_buffer_id);
                user_data.shader_output.push_back(SpvStorageClassUniform);
            }

            switch (parsed_instruction->opcode) {
                case SpvOpEntryPoint:
                    {
                        if (user_data.version < 0x00010400) { // Uniforms are only declared in the OpEntryPoint instruction in SPIR-V 1.4 and later
                            user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words, parsed_instruction->words + parsed_instruction->num_words);
                            break;
                        }
                        // Remove the old uniform buffers from the OpEntryPoint instruction and add our merged uniform buffer to it
                        std::vector<uint32_t> interface;
                        uint16_t num_words = parsed_instruction->num_words;
                        for (uint16_t operand_index = 3; operand_index < parsed_instruction->num_operands; ++operand_index) {
                            assert(parsed_instruction->operands[operand_index].num_words == 1);
                            uint32_t id = parsed_instruction->words[parsed_instruction->operands[operand_index].offset];
                            if (user_data.uniform_trie.children.count(id)) {
                                --num_words;
                            } else {
                                interface.push_back(id);
                            }
                        }
                        interface.push_back(user_data.merged_uniform_buffer_id);
                        num_words += 1;
                        push_instruction(user_data.shader_output, SpvOpEntryPoint, num_words);
                        for (uint16_t operand_index = 0; operand_index < 3; ++operand_index) {
                            user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words + parsed_instruction->operands[operand_index].offset, parsed_instruction->words + parsed_instruction->operands[operand_index].offset + parsed_instruction->operands[operand_index].num_words);
                        }
                        user_data.shader_output.insert(user_data.shader_output.end(), interface.begin(), interface.end());
                    }
                    break;

                case SpvOpVariable:
                    // Remove the declarations of the old uniform buffers
                    if (!user_data.uniform_trie.children.count(parsed_instruction->words[2])) {
                        user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words, parsed_instruction->words + parsed_instruction->num_words);
                    }
                    break;

                case SpvOpName:
                    // Remove debug information from the old uniform buffers
                    if (!user_data.uniform_trie.children.count(parsed_instruction->words[1])) {
                        user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words, parsed_instruction->words + parsed_instruction->num_words);
                    }
                    break;

                case SpvOpDecorate:
                case SpvOpGroupDecorate:
                case SpvOpDecorateId:
                case SpvOpDecorateString:
                    // Remove annotations from the old uniform buffers
                    if (!user_data.uniform_trie.children.count(parsed_instruction->words[1])) {
                        user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words, parsed_instruction->words + parsed_instruction->num_words);
                    }
                    break;

                // TODO: there are several more instructions that can read from uniform buffers
                case SpvOpAccessChain:
                case SpvOpInBoundsAccessChain:
                    {
                        // Rewrite any instructions that try to read from one of the old uniform buffers such that they instead read from the merged uniform buffer
                        uint32_t base = parsed_instruction->words[3];
                        if (!user_data.uniform_trie.children.count(base) && !user_data.pointers.count(base)) {
                            user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words, parsed_instruction->words + parsed_instruction->num_words);
                            break;
                        }
                        const UniformTrieNode *node = user_data.uniform_trie.children.count(base) ? user_data.uniform_trie.children.at(base).get() : &user_data.pointers.at(base);
                        std::vector<uint32_t> extra_chain_words;
                        for (size_t word_index = 4; word_index < parsed_instruction->num_words; ++word_index) {
                            uint32_t chain_word = parsed_instruction->words[word_index];
                            if (node->children.empty()) {
                                extra_chain_words.push_back(chain_word);
                            } else {
                                uint32_t constant = user_data.constants.at(parsed_instruction->words[word_index]);
                                node = node->children.at(constant).get();
                            }
                        }
                        uint32_t result = parsed_instruction->words[2];
                        if (!node->children.empty()) {
                            user_data.pointers.emplace(result, *node);
                        } else {
                            uint32_t result_type = parsed_instruction->words[1];
                            push_instruction(user_data.shader_output, SpvOpInBoundsAccessChain, (uint16_t)extra_chain_words.size() + 5);
                            user_data.shader_output.push_back(result_type);
                            user_data.shader_output.push_back(result);
                            user_data.shader_output.push_back(user_data.merged_uniform_buffer_id);
                            user_data.shader_output.push_back(user_data.merged_uniform_buffer_member_index_ids[user_data.uniform_index_map.at(node->uniform_name)]);
                            user_data.shader_output.insert(user_data.shader_output.end(), extra_chain_words.begin(), extra_chain_words.end());
                        }
                    }
                    break;

                default:
                    user_data.shader_output.insert(user_data.shader_output.end(), parsed_instruction->words, parsed_instruction->words + parsed_instruction->num_words);
                    break;
            }

            return SPV_SUCCESS;
        },
        &diagnostic
    );
    if (result != SPV_SUCCESS) {
        if (!user_data.error_message.empty()) {
            error_message = std::move(user_data.error_message);
        } else {
            error_message = diagnostic->error;
        }
        spvDiagnosticDestroy(diagnostic);
        return false;
    }
    return true;
}

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
    std::vector<const std::pair<const uint32_t *, size_t> *> shader_for_execution_model(sizeof execution_models / sizeof *execution_models, nullptr);
    std::unordered_map<std::string, SpirvType> uniform_map;
    std::vector<UniformTrieNode> uniform_trie_for_execution_model(sizeof execution_models / sizeof *execution_models);

    for (size_t shader_index = 0; shader_index < shaders_len; ++shader_index) {
        const std::pair<const uint32_t *, size_t> &shader = shaders[shader_index];

        SpirvExecutionModel execution_model;
        if (!get_execution_model(
            (spvc_context)spirv_cross_context,
            shader.first,
            shader.second,
            execution_model,
            error_message
        )) {
            return false;
        }

        size_t execution_model_index = get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model);
        if (shader_for_execution_model[execution_model_index] != nullptr) {
            error_message = "At most one shader for each SPIR-V execution model can be provided";
            return false;
        }
        shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model)] = &shaders[shader_index];

        if (!get_uniforms(
            backend,
            (spvc_context)spirv_cross_context,
            shader.first,
            shader.second,
            uniform_map,
            uniform_trie_for_execution_model[execution_model_index],
            error_message
        )) {
            return false;
        }
    }

    if (
        shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, SpirvExecutionModel::VERTEX)] == nullptr
            || shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, SpirvExecutionModel::FRAGMENT)] == nullptr
    ) {
        error_message = "A vertex shader and a fragment shader must both be provided";
        return false;
    }

    // Sort the uniforms in non-ascending order of alignment to optimize for the size of the merged uniform buffer.
    std::vector<std::pair<std::string, SpirvType>> uniforms;
    for (auto it = uniform_map.begin(); it != uniform_map.end(); it = uniform_map.erase(it)) {
        uniforms.emplace_back(it->first, std::move(it->second));
    }
    std::sort(uniforms.begin(), uniforms.end(), [this](const std::pair<std::string, SpirvType> &a, const std::pair<std::string, SpirvType> &b) {
        return get_type_layout(backend, a.second).alignment > get_type_layout(backend, b.second).alignment;
    });

    GlShaders gl_shaders;

    std::vector<std::vector<uint32_t>> normalized_shader_for_execution_model(shader_for_execution_model.size());

    // We need to make sure all the shaders in our shader program have exactly the same set of uniforms,
    // so replace all of the uniform buffers in each shader with one merged uniform buffer that contains all of the uniforms used in at least one shader.
    for (size_t execution_model_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        const std::pair<const uint32_t *, size_t> *shader = shader_for_execution_model[execution_model_index];
        if (shader == nullptr) {
            continue;
        }

        if (!rewrite_shader_uniforms(
            backend,
            (spv_context)spirv_tools_context,
            shader->first,
            shader->second,
            uniforms,
            uniform_trie_for_execution_model[execution_model_index],
            normalized_shader_for_execution_model[execution_model_index],
            error_message
        )) {
            return false;
        }
    }

    static std::random_device random_device;
    std::random_device::result_type cookie = random_device();

    for (size_t execution_model_index = 0, stage_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        const std::vector<uint32_t> shader = normalized_shader_for_execution_model[execution_model_index];
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
