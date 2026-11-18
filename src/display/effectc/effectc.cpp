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

#define COMPILE_ERROR do { \
    if (it == pass.second.second.end() || it->second.second.get_type() != Value::Type::SHADER) { \
        LINK_ERROR; \
    } \
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

bool effectc::compile(
    std::unordered_map<std::string, SpirvSink::Program> &programs_output,
    std::string &error_messages,
    HlslToSpirv &hlsl_to_spirv_compiler,
    SpirvSink &sink,
    const uint8_t *default_vertex_shader,
    size_t default_vertex_shader_size,
    bool override_vertex_shader,
    const uint8_t *default_fragment_shader,
    size_t default_fragment_shader_size,
    bool override_fragment_shader,
    const char *shaders_source_code,
    const char *passes_source_code,
    size_t shaders_preamble_size
) {
    std::unordered_map<std::string, SpirvSink::Program> new_programs;

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

    std::vector<std::vector<uint8_t>> shaders;

    for (const auto &pass : passes) {
        static const std::pair<const char *, SpirvExecutionModel> execution_models[] = {
            {"VERTEXSHADER", SpirvExecutionModel::VERTEX},
            {"PIXELSHADER", SpirvExecutionModel::FRAGMENT},
        };

        for (size_t execution_model_index = 0; execution_model_index < sizeof execution_models / sizeof *execution_models; ++execution_model_index) {
            const std::pair<const char *, SpirvExecutionModel> &execution_model = execution_models[execution_model_index];
            auto it = pass.second.second.find(execution_model.first);

            bool have_default_shader = (execution_model.second == SpirvExecutionModel::VERTEX ? default_vertex_shader : default_fragment_shader) != nullptr;
            bool override_shader = have_default_shader && (execution_model.second == SpirvExecutionModel::VERTEX ? override_vertex_shader : override_fragment_shader);

            if (!override_shader && it != pass.second.second.end() && it->second.second.get_type() == Value::Type::SHADER) {
                std::vector<uint8_t> shader;
                COMPILE_CHECK(hlsl_to_spirv_compiler.compile(
                    shader,
                    errors,
                    shaders_source_code,
                    it->second.second.as_shader().entry_point.c_str(),
                    shaders_preamble_size,
                    execution_model.second,
                    HlslToSpirv::Dialect::D3D9
                ));
                shaders.emplace_back(std::move(shader));
            } else if (have_default_shader) {
                const uint8_t *shader_ptr = execution_model.second == SpirvExecutionModel::VERTEX ? default_vertex_shader : default_fragment_shader;
                shaders.emplace_back(shader_ptr, shader_ptr + (execution_model.second == SpirvExecutionModel::VERTEX ? default_vertex_shader_size : default_fragment_shader_size));
            } else {
                errors = execution_model.second == SpirvExecutionModel::VERTEX ? "You must specify a vertex shader for this pass." : "You must specify a fragment shader for this pass.";
                COMPILE_ERROR;
            }
        }

        std::vector<std::pair<const uint8_t *, size_t>> shader_views(shaders.size());
        for (size_t i = 0; i < shaders.size(); ++i) {
            const std::vector<uint8_t> &shader = shaders[i];
            shader_views[i] = std::make_pair(shader.data(), shader.size());
        }
        SpirvSink::Program program;
        LINK_CHECK(sink.compile(
            program,
            errors,
            shader_views.data(),
            shader_views.size()
        ));

        new_programs.emplace(pass.first, std::move(program));
    }

    programs_output = std::move(new_programs);
    return true;
}
