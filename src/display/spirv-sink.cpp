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

#include <cassert>
#include <random>
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

bool SpirvSink::compile(
    Program &program_output,
    std::string &error_message,
    const std::pair<const uint8_t *, size_t> *shaders,
    size_t shaders_len
) {
    static const SpirvExecutionModel execution_models[] = {
        SpirvExecutionModel::VERTEX,
        SpirvExecutionModel::TESSELLATION_CONTROL,
        SpirvExecutionModel::TESSELLATION_EVALUATION,
        SpirvExecutionModel::GEOMETRY,
        SpirvExecutionModel::FRAGMENT,
    };
    std::vector<const std::pair<const uint8_t *, size_t> *> shader_for_execution_model(sizeof execution_models / sizeof *execution_models, nullptr);

    for (size_t shader_index = 0; shader_index < shaders_len; ++shader_index) {
        const std::pair<const uint8_t *, size_t> &shader = shaders[shader_index];

        assert((uintptr_t)shader.first % (uintptr_t)alignof(uint32_t) == 0); // Should be true because memory allocated by malloc() should be sufficiently aligned for uint32_t
        if (shader.second % 4 != 0) {
            error_message = "SPIR-V input size must be a multiple of 4";
            return false;
        }

        spvc_parsed_ir ir;
        if (spvc_context_parse_spirv((spvc_context)context, (const SpvId *)shader.first, shader.second / 4, &ir) != SPVC_SUCCESS) {
            error_message = spvc_context_get_last_error_string((spvc_context)context);
            spvc_context_release_allocations((spvc_context)context);
            return false;
        }

        spvc_compiler compiler;
        if (spvc_context_create_compiler((spvc_context)context, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS) {
            error_message = spvc_context_get_last_error_string((spvc_context)context);
            spvc_context_release_allocations((spvc_context)context);
            return false;
        }

        SpvExecutionModel compiler_execution_model = spvc_compiler_get_execution_model(compiler);

        spvc_context_release_allocations((spvc_context)context);

        SpirvExecutionModel execution_model;
        switch (compiler_execution_model) {
            case SpvExecutionModelVertex: execution_model = SpirvExecutionModel::VERTEX; break;
            case SpvExecutionModelTessellationControl: execution_model = SpirvExecutionModel::TESSELLATION_CONTROL; break;
            case SpvExecutionModelTessellationEvaluation: execution_model = SpirvExecutionModel::TESSELLATION_EVALUATION; break;
            case SpvExecutionModelGeometry: execution_model = SpirvExecutionModel::GEOMETRY; break;
            case SpvExecutionModelFragment: execution_model = SpirvExecutionModel::FRAGMENT; break;
            default:
                error_message = "Unsupported SPIR-V execution model " + std::to_string(execution_model);
                return false;
        }

        size_t execution_model_index = get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model);
        if (shader_for_execution_model[execution_model_index] != nullptr) {
            error_message = "At most one shader for each SPIR-V execution model can be provided";
            return false;
        }
        shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, execution_model)] = &shaders[shader_index];
    }

    if (
        shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, SpirvExecutionModel::VERTEX)] == nullptr
            || shader_for_execution_model[get_execution_model_index(execution_models, sizeof execution_models / sizeof *execution_models, SpirvExecutionModel::FRAGMENT)] == nullptr
    ) {
        error_message = "A vertex shader and a fragment shader must both be provided";
        return false;
    }

    GlShaders gl_shaders;

    static std::random_device random_device;
    std::random_device::result_type cookie = random_device();

    for (size_t execution_model_index = 0, stage_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
        const std::pair<const uint8_t *, size_t> *shader = shader_for_execution_model[execution_model_index];
        SpirvExecutionModel execution_model = execution_models[execution_model_index];
        if (shader == nullptr) {
            continue;
        }

        SpirvShaderInfo info;

        switch (backend) {
            case Backend::OPENGL_GLSL:
                {
                    std::string glsl;
                    bool result = glsl_backend.compile(
                        glsl,
                        info,
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
