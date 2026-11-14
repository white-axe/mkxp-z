/*
 ** effectc.cpp
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

#include <limits>
#include <sstream>
#include <utility>

#include "effectc.h"
#include "gl-fun.h"
#include "impl.h"

using namespace effectc_impl;

std::string effectc_impl::format_location(const Parser::location_type &location) {
    std::stringstream formatted_location;
    if (location.begin.filename != nullptr) {
        formatted_location << "Error in " << location.begin.filename << ", on ";
    } else {
        formatted_location << "Error on ";
    }
    decltype(location.end.column) end_column_minus_one = location.end.column > std::numeric_limits<decltype(location.end.column)>::min() ? location.end.column - 1 : std::numeric_limits<decltype(location.end.column)>::min();
    if (location.begin.line < location.end.line) {
        formatted_location << "line " << location.begin.line << ", column " << location.begin.column << " to line " << location.end.line << ", column " << end_column_minus_one;
    } else if (location.begin.column < end_column_minus_one) {
        formatted_location << "line " << location.begin.line << ", columns " << location.begin.column << '-' << end_column_minus_one;
    } else {
        formatted_location << "line " << location.begin.line << ", column " << location.begin.column;
    }
    return formatted_location.str();
}

Effectc::Program::Program() : vertex_shader_index(0), fragment_shader_index(0), program_index(0) {
    static_assert(sizeof vertex_shader_index == sizeof(GLuint), "vertex_shader_index should be the same size as GLuint, but it isn't");
    static_assert(sizeof fragment_shader_index == sizeof(GLuint), "fragment_shader_index should be the same size as GLuint, but it isn't");
    static_assert(sizeof program_index == sizeof(GLuint), "program_index should be the same size as GLuint, but it isn't");
}

Effectc::Program::~Program() {
    if (program_index != 0) {
        gl.DeleteProgram((GLuint)program_index);
    }
    if (fragment_shader_index != 0) {
        gl.DeleteShader((GLuint)fragment_shader_index);
    }
    if (vertex_shader_index != 0) {
        gl.DeleteShader((GLuint)vertex_shader_index);
    }
}

Effectc::Program::Program(Program &&other) noexcept {
    *this = std::move(other);
}

Effectc::Program &Effectc::Program::operator=(Program &&other) noexcept {
    vertex_shader_index = other.vertex_shader_index;
    other.vertex_shader_index = 0;
    fragment_shader_index = other.fragment_shader_index;
    other.fragment_shader_index = 0;
    program_index = other.program_index;
    other.program_index = 0;
    ubos = std::move(other.ubos);
    uniforms = std::move(other.uniforms);
    return *this;
}

#define COMPILE_ERROR do { \
    std::stringstream formatted_errors; \
    formatted_errors << format_location(it->second.first) << ": failed to compile shader " << it->second.second.as_shader().entry_point << "():" << std::endl; \
    if (errors.empty()) { \
        formatted_errors << "    there is an unknown problem with your graphics driver" << std::endl; \
    } else { \
        std::istringstream errors_stream(errors); \
        std::string errors_line; \
        while (std::getline(errors_stream, errors_line)) { \
            if (errors_line.empty()) { \
                continue; \
            } \
            formatted_errors << "    " << errors_line << std::endl; \
        } \
    } \
    error_messages = formatted_errors.str(); \
    return false; \
} while (0)

#define COMPILE_CHECK(value) do { \
    if (!(value)) { \
        COMPILE_ERROR; \
    } \
} while (0)

#define LINK_ERROR do { \
    std::stringstream formatted_errors; \
    formatted_errors << format_location(pass.second.first) << ": failed to link shader program " << pass.first << ":" << std::endl; \
    if (errors.empty()) { \
        formatted_errors << "    there is an unknown problem with your graphics driver" << std::endl; \
    } else { \
        std::istringstream errors_stream(errors); \
        std::string errors_line; \
        while (std::getline(errors_stream, errors_line)) { \
            if (errors_line.empty()) { \
                continue; \
            } \
            formatted_errors << "    " << errors_line << std::endl; \
        } \
    } \
    error_messages = formatted_errors.str(); \
    return false; \
} while (0)

#define LINK_CHECK(value) do { \
    if (!(value)) { \
        LINK_ERROR; \
    } \
} while (0)

bool Effectc::compile(std::string &error_messages, HlslToSpirv &hlsl_to_spirv_compiler, SpirvOptimizer &optimizer, SpirvToGlsl &spirv_to_glsl_compiler, const char *shaders_source_code, const char *passes_source_code, int shaders_line_number_offset) {
    std::unordered_map<std::string, Program> new_programs;

    std::string errors;
    std::unordered_map<std::string, std::pair<Parser::location_type, std::unordered_map<std::string, std::pair<Parser::location_type, Value>>>> passes;

    {
        Lexer lexer(passes_source_code);
        Parser parser(passes, errors, lexer);
        if (parser.parse() != 0) {
            error_messages = std::move(errors);
            return false;
        }
    }

    for (const auto &pass : passes) {
        static const std::pair<const char *, HlslToSpirv::Stage> stages[] = {
            {"VERTEXSHADER", HlslToSpirv::Stage::VERTEX},
            {"PIXELSHADER", HlslToSpirv::Stage::FRAGMENT},
        };

        Program program;
        GLint gl_status;

        for (size_t stage_index = 0; stage_index < sizeof stages / sizeof *stages; ++stage_index) {
            const std::pair<const char *, HlslToSpirv::Stage> &stage = stages[stage_index];
            auto it = pass.second.second.find(stage.first);
            if (it == pass.second.second.end() || it->second.second.get_type() != Value::Type::SHADER) {
                continue;
            }

            uint32_t &shader_index = stage.second == HlslToSpirv::Stage::VERTEX ? program.vertex_shader_index : program.fragment_shader_index;
            std::string glsl;

            {
                std::vector<uint8_t> optimized_spirv;
                {
                    std::vector<uint8_t> unoptimized_spirv;
                    COMPILE_CHECK(hlsl_to_spirv_compiler.compile(unoptimized_spirv, errors, shaders_source_code, it->second.second.as_shader().entry_point.c_str(), shaders_line_number_offset, stage.second, HlslToSpirv::Dialect::D3D9));
                    COMPILE_CHECK(optimizer.optimize(optimized_spirv, unoptimized_spirv.data(), unoptimized_spirv.size()));
                }
                COMPILE_CHECK(spirv_to_glsl_compiler.compile(glsl, program.ubos, program.uniforms, errors, optimized_spirv.data(), optimized_spirv.size(), 300, SpirvToGlsl::Dialect::GLSL_ES)); // TODO: automatically determine version and dialect
            }

            COMPILE_CHECK((shader_index = gl.CreateShader(stage.second == HlslToSpirv::Stage::VERTEX ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER)) != 0);
            const char *glsl_c_str = glsl.c_str();
            gl.ShaderSource(shader_index, 1, &glsl_c_str, nullptr);
            gl.CompileShader(shader_index);
            gl.GetShaderiv(shader_index, GL_COMPILE_STATUS, &gl_status);
            if (gl_status != GL_TRUE) {
                GLint log_length;
                gl.GetShaderiv(shader_index, GL_INFO_LOG_LENGTH, &log_length);
                errors.resize((size_t)log_length);
                gl.GetShaderInfoLog(shader_index, log_length, nullptr, (GLchar *)&errors[0]);
                COMPILE_ERROR;
            }
        }

        LINK_CHECK((program.program_index = gl.CreateProgram()) != 0);
        if (program.vertex_shader_index != 0) {
            gl.AttachShader(program.program_index, program.vertex_shader_index);
        }
        if (program.fragment_shader_index != 0) {
            gl.AttachShader(program.program_index, program.fragment_shader_index);
        }
        gl.LinkProgram(program.program_index);
        gl.GetProgramiv(program.program_index, GL_LINK_STATUS, &gl_status);
        if (gl_status != GL_TRUE) {
            GLint log_length;
            gl.GetProgramiv(program.program_index, GL_INFO_LOG_LENGTH, &log_length);
            errors.resize((size_t)log_length);
            gl.GetProgramInfoLog(program.program_index, log_length, nullptr, (GLchar *)&errors[0]);
            LINK_ERROR;
        }

        if (program.fragment_shader_index != 0) {
            gl.DeleteShader(program.fragment_shader_index);
            program.fragment_shader_index = 0;
        }
        if (program.vertex_shader_index != 0) {
            gl.DeleteShader(program.vertex_shader_index);
            program.vertex_shader_index = 0;
        }

        new_programs.emplace(pass.first, std::move(program));
    }

    programs = std::move(new_programs);
    return true;
}
