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

#include "effectc.h"
#include "impl.h"

using namespace effectc_impl;

bool effectc::compile(std::string &error_messages, HlslToSpirv &hlsl_to_spirv_compiler, SpirvOptimizer &optimizer, const char *shaders_source_code, const char *passes_source_code, int shaders_line_number_offset) {
    std::unordered_map<std::string, std::unordered_map<std::string, effectc_impl::Value>> passes;
    {
        std::string errors;
        Lexer lexer(passes_source_code);
        Parser parser(passes, errors, hlsl_to_spirv_compiler, shaders_source_code, lexer);
        if (parser.parse() != 0) {
            error_messages = std::move(errors);
            return false;
        }
    }

    return true;
}
