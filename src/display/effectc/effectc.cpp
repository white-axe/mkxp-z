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

#include <sstream>
#include <utility>

#include "effectc.h"
#include "impl.h"

using namespace effectc_impl;

std::string effectc_impl::format_location(const Parser::location_type location) {
    std::stringstream formatted_location;
    if (location.begin.filename != nullptr) {
        formatted_location << "Error in " << location.begin.filename << ", on ";
    } else {
        formatted_location << "Error on ";
    }
    if (location.begin.line != location.end.line) {
        formatted_location << "line " << location.begin.line << ", column " << location.begin.column << " to line " << location.end.line << ", column " << location.end.column;
    } else if (location.begin.column != location.end.column) {
        formatted_location << "line " << location.begin.line << ", columns " << location.begin.column << '-' << location.end.column;
    } else {
        formatted_location << "line " << location.begin.line << ", column " << location.begin.column;
    }
    return formatted_location.str();
}

bool effectc::compile(std::string &error_messages, HlslToSpirv &hlsl_to_spirv_compiler, SpirvOptimizer &optimizer, const char *shaders_source_code, const char *passes_source_code, int shaders_line_number_offset) {
    std::string errors;
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<Parser::location_type, Value>>> passes;

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

        for (size_t stage_index = 0; stage_index < sizeof stages / sizeof *stages; ++stage_index) {
            const std::pair<const char *, HlslToSpirv::Stage> &stage = stages[stage_index];
            auto it = pass.second.find(stage.first);
            if (it == pass.second.end() || it->second.second.get_type() != Value::Type::SHADER) {
                continue;
            }

            std::vector<uint8_t> unoptimized_spirv;
            if (!hlsl_to_spirv_compiler.compile(unoptimized_spirv, errors, shaders_source_code, it->second.second.as_shader().entry_point.c_str(), shaders_line_number_offset, stage.second, HlslToSpirv::Dialect::D3D9)) {
                std::stringstream formatted_errors;
                formatted_errors << format_location(it->second.first) << ": failed to compile shader " << it->second.second.as_shader().entry_point << "():";
                std::istringstream errors_stream(errors);
                std::string errors_line;
                while (std::getline(errors_stream, errors_line)) {
                    if (errors_line.empty()) {
                        continue;
                    }
                    formatted_errors << std::endl << "    " << errors_line;
                }
                error_messages = formatted_errors.str();
                return false;
            }

            std::vector<uint8_t> optimized_spirv; // TODO: do something with this
            if (!optimizer.optimize(optimized_spirv, unoptimized_spirv.data(), unoptimized_spirv.size())) {
                std::stringstream formatted_errors;
                formatted_errors << format_location(it->second.first) << ": failed to compile shader " << it->second.second.as_shader().entry_point << "():";
                std::istringstream errors_stream(errors);
                std::string errors_line;
                while (std::getline(errors_stream, errors_line)) {
                    if (errors_line.empty()) {
                        continue;
                    }
                    formatted_errors << std::endl << "    " << errors_line;
                }
                error_messages = formatted_errors.str();
                return false;
            }
        }
    }

    return true;
}
