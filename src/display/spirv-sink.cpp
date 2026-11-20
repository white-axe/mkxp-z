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
#include <cstring>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "exception.h"
#include "gl-fun.h"
#include "spirv-sink.h"

#include <spirv_cross_c.h>

static SpirvTarget get_target(SpirvSink::Backend backend) noexcept {
    switch (backend) {
        case SpirvSink::Backend::OPENGL_GLSL: return SpirvToGlsl::TARGET;
    }
}

struct TypeSizeAndAlignment
{
    // Size of the type in bytes, including padding bytes.
    uint32_t size;
    // Alignment of the type in bytes.
    uint32_t alignment;
};

static TypeSizeAndAlignment get_type_size_and_alignment(SpirvSink::Backend backend, const SpirvType &type) noexcept {
    if (type.array_dimensions.empty()) {
        if (type.columns == 1) {
            if (type.vector_size == 1) { // Scalar
                switch (type.scalar_type) {
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
                SpirvType scalar_type = type;
                scalar_type.vector_size = 1;
                TypeSizeAndAlignment scalar_size_and_alignment = get_type_size_and_alignment(backend, scalar_type);
                scalar_size_and_alignment.alignment *= type.vector_size == 3 ? 4 : type.vector_size; // A 3-component vector has its alignment rounded up to 4 components
                scalar_size_and_alignment.size = scalar_size_and_alignment.alignment;
                return scalar_size_and_alignment;
            }
        } else if (type.is_row_major) { // Row-major matrix
            SpirvType vector_array_type = type;
            vector_array_type.columns = 1;
            vector_array_type.vector_size = type.columns;
            vector_array_type.array_dimensions = {type.vector_size};
            return get_type_size_and_alignment(backend, vector_array_type);
        } else { // Column-major matrix
            SpirvType vector_array_type = type;
            vector_array_type.columns = 1;
            vector_array_type.array_dimensions = {type.columns};
            return get_type_size_and_alignment(backend, vector_array_type);
        }
    } else if (type.columns == 1) { // Array of scalars or vectors
        uint32_t array_size = 1;
        for (uint32_t dimension : type.array_dimensions) {
            array_size *= dimension;
        }
        SpirvType vector_type = type;
        vector_type.array_dimensions = {};
        TypeSizeAndAlignment vector_size_and_alignment = get_type_size_and_alignment(backend, vector_type);
        // Round the size and alignment of each element of the array up to the nearest multiple of 16
        vector_size_and_alignment.size = (((uint64_t)vector_size_and_alignment.size + (uint64_t)15) & ~(uint64_t)15);
        vector_size_and_alignment.alignment = (((uint64_t)vector_size_and_alignment.alignment + (uint64_t)15) & ~(uint64_t)15);
        vector_size_and_alignment.size *= array_size;
        return vector_size_and_alignment;
    } else if (type.is_row_major) { // Array of row-major matrices
        uint32_t array_size = 1;
        for (uint32_t dimension : type.array_dimensions) {
            array_size *= dimension;
        }
        SpirvType vector_array_type = type;
        vector_array_type.columns = 1;
        vector_array_type.vector_size = type.columns;
        vector_array_type.array_dimensions = {array_size * type.vector_size};
        return get_type_size_and_alignment(backend, vector_array_type);
    } else { // Array of column-major matrices
        uint32_t array_size = 1;
        for (uint32_t dimension : type.array_dimensions) {
            array_size *= dimension;
        }
        SpirvType vector_array_type = type;
        vector_array_type.columns = 1;
        vector_array_type.array_dimensions = {array_size * type.columns};
        return get_type_size_and_alignment(backend, vector_array_type);
    }
}

SpirvSink::SpirvSink(Backend backend) : context(nullptr), backend(backend), optimizer(get_target(backend)) {
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

SpirvSink::SpirvSink(SpirvSink &&other) noexcept : context(other.context), backend(other.backend), optimizer(std::move(other.optimizer)) {
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

static bool get_execution_model(SpirvExecutionModel &execution_model, std::string &error_message, spvc_context context, const uint32_t *shader, size_t shader_size) {
    spvc_parsed_ir ir;
    SPVC_CHECK(spvc_context_parse_spirv(context, (const SpvId *)shader, shader_size, &ir));

    spvc_compiler compiler;
    SPVC_CHECK(spvc_context_create_compiler(context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    SpvExecutionModel compiler_execution_model = spvc_compiler_get_execution_model(compiler);

    spvc_context_release_allocations(context);

    switch (compiler_execution_model) {
        case SpvExecutionModelVertex: execution_model = SpirvExecutionModel::VERTEX; break;
        case SpvExecutionModelTessellationControl: execution_model = SpirvExecutionModel::TESSELLATION_CONTROL; break;
        case SpvExecutionModelTessellationEvaluation: execution_model = SpirvExecutionModel::TESSELLATION_EVALUATION; break;
        case SpvExecutionModelGeometry: execution_model = SpirvExecutionModel::GEOMETRY; break;
        case SpvExecutionModelFragment: execution_model = SpirvExecutionModel::FRAGMENT; break;
        default:
            error_message = "Unsupported SPIR-V execution model " + std::to_string(compiler_execution_model);
            return false;
    }
    return true;
}

static bool get_uniform_types(std::unordered_map<std::string, SpirvType> &uniform_types, std::string &error_message, spvc_context context, const uint32_t *shader, size_t shader_size) {
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
        stack.emplace_back(Node {
            uniform_buffer_name,
            uniform_buffer_list[uniform_buffer_index].base_type_id,
            0,
        });
        while (!stack.empty()) {
            Node &node = stack.back();
            spvc_type type = spvc_compiler_get_type_handle(compiler, node.type_id);
            spvc_basetype basetype = spvc_type_get_basetype(type);
            if (basetype == SPVC_BASETYPE_STRUCT) {
                if (node.member_index < (size_t)spvc_type_get_num_member_types(type)) {
                    size_t member_index = node.member_index++;
                    stack.emplace_back(Node {
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
                uniform_type.is_row_major = uniform_type.columns != 1 && spvc_compiler_get_decoration(compiler, node.type_id, SpvDecorationRowMajor) != 0;
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
                uniform_types.emplace(std::move(uniform_name), std::move(uniform_type));
                stack.pop_back();
            }
        }
    }

    spvc_context_release_allocations(context);
    return true;
}

bool SpirvSink::compile(
    Program &program_output,
    std::string &error_message,
    const std::pair<const uint32_t *, size_t> *shaders,
    size_t shaders_len
) {
    static const SpirvExecutionModel execution_models[] = {
        SpirvExecutionModel::VERTEX,
        SpirvExecutionModel::TESSELLATION_CONTROL,
        SpirvExecutionModel::TESSELLATION_EVALUATION,
        SpirvExecutionModel::GEOMETRY,
        SpirvExecutionModel::FRAGMENT,
    };
    std::vector<const std::pair<const uint32_t *, size_t> *> shader_for_execution_model(sizeof execution_models / sizeof *execution_models, nullptr);
    std::unordered_map<std::string, SpirvType> uniform_type_map;

    for (size_t shader_index = 0; shader_index < shaders_len; ++shader_index) {
        const std::pair<const uint32_t *, size_t> &shader = shaders[shader_index];

        SpirvExecutionModel execution_model;
        if (!get_execution_model(execution_model, error_message, (spvc_context)context, shader.first, shader.second)) {
            return false;
        }

        size_t execution_model_index = get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model);
        if (shader_for_execution_model[execution_model_index] != nullptr) {
            error_message = "At most one shader for each SPIR-V execution model can be provided";
            return false;
        }
        shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model)] = &shaders[shader_index];

        if (!get_uniform_types(uniform_type_map, error_message, (spvc_context)context, shader.first, shader.second)) {
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

    // Sort the uniform types in non-ascending order of alignment to optimize for the size of the uniform buffer
    std::vector<std::pair<std::string, SpirvType>> uniform_types;
    for (auto it = uniform_type_map.begin(); it != uniform_type_map.end(); it = uniform_type_map.erase(it)) {
        uniform_types.emplace_back(it->first, std::move(it->second));
    }
    std::sort(uniform_types.begin(), uniform_types.end(), [this](const std::pair<std::string, SpirvType> &a, const std::pair<std::string, SpirvType> &b) {
        return get_type_size_and_alignment(backend, a.second).alignment > get_type_size_and_alignment(backend, b.second).alignment;
    });

    GlShaders gl_shaders;

    static std::random_device random_device;
    std::random_device::result_type cookie = random_device();

    for (size_t execution_model_index = 0, stage_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        const std::pair<const uint32_t *, size_t> *shader = shader_for_execution_model[execution_model_index];
        SpirvExecutionModel execution_model = execution_models[execution_model_index];
        if (shader == nullptr) {
            continue;
        }

        switch (backend) {
            case Backend::OPENGL_GLSL:
                {
                    std::string glsl;
                    bool result = glsl_backend.compile(
                        glsl,
                        error_message,
                        shader->first,
                        shader->second,
                        (std::to_string(cookie) + "_stage" + std::to_string(stage_index)).c_str(),
                        (std::to_string(cookie) + "_stage" + std::to_string(stage_index + 1)).c_str(),
                        gl.glsl_info.version,
                        gl.glsl_info.es ? SpirvToGlsl::Dialect::GLSL_ES : gl.glsl_info.have_420pack ? SpirvToGlsl::Dialect::GLSL_WITH_420PACK : SpirvToGlsl::Dialect::GLSL_WITHOUT_420PACK
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
