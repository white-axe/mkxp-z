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
#include "spirv-transform.h"

#include <spirv/unified1/spirv.h>
#include <spirv_cross_c.h>

static SpirvTarget get_target(SpirvSink::Backend backend) noexcept {
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: return SpirvToGlsl::TARGET;
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

SpirvSink::SpirvSink(Backend backend) : context(nullptr), backend(backend), legalization_transform(SpirvOptimizer::Type::LEGALIZATION), performance_transform(SpirvOptimizer::Type::PERFORMANCE) {
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

SpirvSink::SpirvSink(SpirvSink &&other) noexcept : context(other.context), backend(other.backend), legalization_transform(std::move(other.legalization_transform)), performance_transform(std::move(other.performance_transform)) {
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

static bool rewrite_shader_uniforms(
    SpirvSink::Backend backend,
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

    SpirvType int32_type;
    int32_type.scalar_type = SpirvType::ScalarType::INT32;
    int32_type.vector_size = 1;
    int32_type.columns = 1;
    int32_type.is_row_major = false;

    std::vector<SpirvType> types;
    for (const std::pair<std::string, SpirvType> &uniform : uniforms) {
        types.emplace_back(uniform.second);
    }
    types.emplace_back(std::move(int32_type));
    std::vector<std::vector<uint32_t>> array_strides;
    for (const SpirvType &type : types) {
        array_strides.emplace_back(get_type_layout(backend, type).array_strides);
    }

    class MergeUniformsTransform : public SpirvTransform
    {
    private:
        SpirvSink::Backend backend;
        const std::vector<std::pair<std::string, SpirvType>> &uniforms;
        const std::unordered_map<std::string, uint32_t> &uniform_index_map;
        const UniformTrieNode &uniform_trie;

        // SPIR-V ID for our merged uniform buffer.
        uint32_t merged_uniform_buffer_id;
        // SPIR-V ID for the struct type for our merged uniform buffer.
        uint32_t merged_uniform_buffer_type_id;
        // SPIR-V ID for a pointer type that points to the struct type for our merged uniform buffer.
        uint32_t merged_uniform_buffer_pointer_type_id;
        // For each uniform in our merged uniform buffer, this contains a SPIR-V ID for a constant used to access the corresponding uniform.
        std::vector<uint32_t> merged_uniform_buffer_index_ids;
        // A map from the SPIR-V IDs of the integer constants defined in the SPIR-V module to the values of those constants.
        std::unordered_map<uint32_t, uint32_t> constants;
        // A map from the SPIR-V IDs for pointer values that refer to some section of a uniform buffer to the uniform trie node for that pointer value.
        std::unordered_map<uint32_t, const UniformTrieNode &> pointers;

    public:
        MergeUniformsTransform(SpirvSink::Backend backend, const std::vector<std::pair<std::string, SpirvType>> &uniforms, const std::unordered_map<std::string, uint32_t> &uniform_index_map, const UniformTrieNode &uniform_trie) : backend(backend), uniforms(uniforms), uniform_index_map(uniform_index_map), uniform_trie(uniform_trie) {
        }

    private:
        uint32_t maximum_supported_spirv_version() const noexcept override {
            return 0x00010600;
        }

        bool begin(std::string &error_message) override {
            if (!allocate_id(merged_uniform_buffer_id, error_message)) {
                return false;
            }
            if (!allocate_id(merged_uniform_buffer_type_id, error_message)) {
                return false;
            }
            if (!allocate_id(merged_uniform_buffer_pointer_type_id, error_message)) {
                return false;
            }
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                uint32_t index_id;
                if (!allocate_id(index_id, error_message)) {
                    return false;
                }
                merged_uniform_buffer_index_ids.push_back(index_id);
            }

            return true;
        }

        bool begin_name_debug_section(std::string &error_message) override {
            // Declare the name of our merged uniform buffer
            begin_instruction(SpvOpName);
            push_word(merged_uniform_buffer_id);
            push_string_literal("mkxpz_spirv_sink_uniforms"); // Don't have to worry about name collisions; the SPIR-V to native shading language compiler will deal with it automatically
            end_instruction();

            // Declare the name of our merged uniform buffer's type
            begin_instruction(SpvOpName);
            push_word(merged_uniform_buffer_type_id);
            push_string_literal("mkxpz_spirv_sink_uniforms_t");
            end_instruction();

            // Declare the names of our merged uniform buffer's fields
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                begin_instruction(SpvOpMemberName);
                push_word(merged_uniform_buffer_type_id);
                push_word(uniform_index);
                push_string_literal(uniforms[uniform_index].first.c_str());
                end_instruction();
            }

            return true;
        }

        bool begin_process_debug_section(std::string &error_message) override {
            begin_instruction(SpvOpModuleProcessed);
            push_string_literal("SpirvSink MergeUniformsTransform");
            end_instruction();

            return true;
        }

        bool begin_annotation_section(std::string &error_message) override {
            // Declare that our merged uniform buffer's type is an interface block
            begin_instruction(SpvOpDecorate);
            push_word(merged_uniform_buffer_type_id);
            push_word(SpvDecorationBlock);
            end_instruction();

            // Declare the linkage of our merged uniform buffer
            begin_instruction(SpvOpDecorate);
            push_word(merged_uniform_buffer_id);
            push_word(SpvDecorationBinding);
            push_word(0);
            end_instruction();
            begin_instruction(SpvOpDecorate);
            push_word(merged_uniform_buffer_id);
            push_word(SpvDecorationDescriptorSet);
            push_word(0);
            end_instruction();

            // Declare the offsets for our merged uniform buffer's fields
            uint32_t offset = 0;
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                const SpirvType &type = uniforms[uniform_index].second;
                TypeLayout layout = get_type_layout(backend, type);

                // Round the offset up to the nearest multiple of the alignment
                if (offset % layout.alignment != 0) {
                    offset += layout.alignment - offset % layout.alignment;
                }

                begin_instruction(SpvOpMemberDecorate);
                push_word(merged_uniform_buffer_type_id);
                push_word(uniform_index);
                push_word(SpvDecorationOffset);
                push_word(offset);
                end_instruction();

                // If it's a matrix or array of matrices, also declare whether it's row-major or column-major and the matrix stride
                if (type.columns != 1) {
                    begin_instruction(SpvOpMemberDecorate);
                    push_word(merged_uniform_buffer_type_id);
                    push_word(uniform_index);
                    push_word(type.is_row_major ? SpvDecorationRowMajor : SpvDecorationColMajor);
                    end_instruction();

                    begin_instruction(SpvOpMemberDecorate);
                    push_word(merged_uniform_buffer_type_id);
                    push_word(uniform_index);
                    push_word(SpvDecorationMatrixStride);
                    push_word(layout.matrix_stride);
                    end_instruction();
                }

                // Increase the offset by the size of this type
                offset += layout.size;
            }

            return true;
        }

        bool begin_data_section(std::string &error_message) override {
            // Declare a constant for each member of our merged uniform buffer
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                begin_instruction(SpvOpConstant);
                push_word(get_extra_type_ids().back());
                push_word(merged_uniform_buffer_index_ids[uniform_index]);
                push_word(uniform_index);
                end_instruction();
            }

            // Declare a struct type for our merged uniform buffer
            begin_instruction(SpvOpTypeStruct);
            push_word(merged_uniform_buffer_type_id);
            push_words(get_extra_type_ids().begin(), get_extra_type_ids().begin() + uniforms.size());
            end_instruction();

            // Declare a pointer type for our merged uniform buffer
            begin_instruction(SpvOpTypePointer);
            push_word(merged_uniform_buffer_pointer_type_id);
            push_word(SpvStorageClassUniform);
            push_word(merged_uniform_buffer_type_id);
            end_instruction();

            // Define the merged uniform buffer
            begin_instruction(SpvOpVariable);
            push_word(merged_uniform_buffer_pointer_type_id);
            push_word(merged_uniform_buffer_id);
            push_word(SpvStorageClassUniform);
            end_instruction();

            return true;
        }

        bool receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message) override {
            if (opcode == SpvOpConstant) {
                constants.emplace(words[1], words[2]);
            }

            switch (opcode) {
                case SpvOpEntryPoint:
                    {
                        if (spirv_version() < 0x00010400) { // Uniforms are only declared in the OpEntryPoint instruction in SPIR-V 1.4 and later
                            begin_instruction(opcode);
                            push_words(words, words + num_words);
                            end_instruction();
                            break;
                        }
                        // Remove the old uniform buffers from the OpEntryPoint instruction and add our merged uniform buffer to it
                        begin_instruction(opcode);
                        uint16_t word_index = 2;
                        while ((words[word_index] & 0xff000000) != 0) {
                            ++word_index;
                        }
                        ++word_index;
                        push_words(words, words + word_index);
                        push_word(merged_uniform_buffer_id);
                        for (; word_index < num_words; ++word_index) {
                            uint32_t id = words[word_index];
                            if (!uniform_trie.children.count(id)) {
                                push_word(id);
                            }
                        }
                        end_instruction();
                    }
                    break;

                case SpvOpVariable:
                    // Remove the declarations of the old uniform buffers
                    if (!uniform_trie.children.count(words[1])) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpName:
                case SpvOpDecorate:
                case SpvOpGroupDecorate:
                case SpvOpDecorateId:
                case SpvOpDecorateString:
                    // Remove debug information and annotations from the old uniform buffers
                    if (!uniform_trie.children.count(words[0])) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                // TODO: there are several more instructions that can read from uniform buffers
                case SpvOpAccessChain:
                case SpvOpInBoundsAccessChain:
                    {
                        // Rewrite any instructions that try to read from one of the old uniform buffers such that they instead read from the merged uniform buffer
                        uint32_t base = words[2];
                        const UniformTrieNode *node;
                        {
                            auto trie_it = uniform_trie.children.find(base);
                            auto pointer_it = pointers.find(base);
                            if (trie_it != uniform_trie.children.end()) {
                                assert(pointer_it == pointers.end());
                                node = trie_it->second.get();
                            } else if (pointer_it != pointers.end()) {
                                assert(trie_it == uniform_trie.children.end());
                                node = &pointer_it->second;
                            } else {
                                // This instruction doesn't try to read one of the old uniform buffers, so pass this instruction through without doing anything
                                begin_instruction(opcode);
                                push_words(words, words + num_words);
                                end_instruction();
                                break;
                            }
                        }
                        for (size_t word_index = 3; word_index < num_words; ++word_index) {
                            uint32_t chain_word = words[word_index];
                            if (node->children.empty()) {
                                push_word(chain_word);
                            } else {
                                uint32_t constant = constants.at(chain_word);
                                node = node->children.at(constant).get();
                                if (node->children.empty()) {
                                    begin_instruction(SpvOpInBoundsAccessChain);
                                    push_words(words, words + 2);
                                    push_word(merged_uniform_buffer_id);
                                    push_word(merged_uniform_buffer_index_ids[uniform_index_map.at(node->uniform_name)]);
                                }
                            }
                        }
                        if (node->children.empty()) {
                            end_instruction();
                        } else {
                            pointers.emplace(words[1], *node);
                        }
                    }
                    break;

                default:
                    begin_instruction(opcode);
                    push_words(words, words + num_words);
                    end_instruction();
                    break;
            }

            return true;
        }
    };

    return MergeUniformsTransform {
        backend,
        uniforms,
        uniform_index_map,
        uniform_trie,
    }.transform(
        shader,
        shader_size,
        types.data(),
        array_strides.data(),
        types.size(),
        shader_output,
        error_message
    );
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
    std::vector<std::vector<uint32_t>> shader_for_execution_model(sizeof execution_models / sizeof *execution_models);
    std::unordered_map<std::string, SpirvType> uniform_map;
    std::vector<UniformTrieNode> uniform_trie_for_execution_model(sizeof execution_models / sizeof *execution_models);

    for (size_t shader_index = 0; shader_index < shaders_len; ++shader_index) {
        const std::pair<const uint32_t *, size_t> &unlegalized_shader = shaders[shader_index];

        SpirvExecutionModel execution_model;
        if (!get_execution_model(
            (spvc_context)context,
            unlegalized_shader.first,
            unlegalized_shader.second,
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

        std::vector<uint32_t> &shader = shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model)];
        if (!legalization_transform.transform(
            unlegalized_shader.first,
            unlegalized_shader.second,
            shader,
            error_message
        )) {
            return false;
        }

        if (!get_uniforms(
            backend,
            (spvc_context)context,
            shader.data(),
            shader.size(),
            uniform_map,
            uniform_trie_for_execution_model[execution_model_index],
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
    std::vector<std::pair<std::string, SpirvType>> uniforms;
    for (auto it = uniform_map.begin(); it != uniform_map.end(); it = uniform_map.erase(it)) {
        uniforms.emplace_back(it->first, std::move(it->second));
    }
    std::sort(uniforms.begin(), uniforms.end(), [this](const std::pair<std::string, SpirvType> &a, const std::pair<std::string, SpirvType> &b) {
        return get_type_layout(backend, a.second).alignment > get_type_layout(backend, b.second).alignment;
    });

    GlShaders gl_shaders;

    for (size_t execution_model_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        std::vector<uint32_t> &shader = shader_for_execution_model[execution_model_index];
        if (shader.empty()) {
            continue;
        }

        // We need to make sure all the shaders in our shader program have exactly the same set of uniforms,
        // so replace all of the uniform buffers in each shader with one merged uniform buffer that contains all of the uniforms used in at least one shader.
        {
            std::vector<uint32_t> output;
            if (!rewrite_shader_uniforms(
                backend,
                shader.data(),
                shader.size(),
                uniforms,
                uniform_trie_for_execution_model[execution_model_index],
                output,
                error_message
            )) {
                return false;
            }
            shader = std::move(output);
        }

        {
            std::vector<uint32_t> output;
            if (!performance_transform.transform(
                shader.data(),
                shader.size(),
                output,
                error_message
            )) {
                return false;
            }
            shader = std::move(output);
        }
    }

    static std::random_device random_device;
    std::random_device::result_type cookie = random_device();

    for (size_t execution_model_index = 0, stage_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        const std::vector<uint32_t> shader = shader_for_execution_model[execution_model_index];
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
