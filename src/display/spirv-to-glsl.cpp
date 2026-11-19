/*
 ** spirv-to-glsl.cpp
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
#include <cctype>
#include <cstring>
#include <vector>
#include <utility>

#include "spirv-to-glsl.h"

#include "exception.h"

#include <spirv_cross_c.h>

SpirvToGlsl::SpirvToGlsl() {
    if (spvc_context_create((spvc_context *)&context) != SPVC_SUCCESS) {
        throw Exception(Exception::MKXPError, "Failed to initialize SPIRV-Cross context");
    }
}

SpirvToGlsl::~SpirvToGlsl() {
    spvc_context_destroy((spvc_context)context);
}

SpirvToGlsl::SpirvToGlsl(SpirvToGlsl &&other) noexcept : context(other.context) {
    other.context = nullptr;
}

SpirvToGlsl &SpirvToGlsl::operator=(SpirvToGlsl &&other) noexcept {
    this->~SpirvToGlsl();
    new(this) SpirvToGlsl(std::move(other));
    return *this;
}

#define CHECK(result) do { \
    if ((result) != SPVC_SUCCESS) { \
        error_message = spvc_context_get_last_error_string((spvc_context)context); \
        spvc_context_release_allocations((spvc_context)context); \
        return false; \
    } \
} while (0)

bool SpirvToGlsl::compile(
    std::string &glsl_output,
    std::string &error_message,
    const uint32_t *spirv_input,
    size_t spirv_input_size,
    const char *stage_input_cookie,
    const char *stage_output_cookie,
    uint16_t version,
    Dialect dialect
) {
    bool have_uniform_buffer_support;
    bool have_binding_support;
    switch (dialect) {
        case Dialect::GLSL_WITH_420PACK:
            have_uniform_buffer_support = version >= 150; // Actually supported starting from GLSL 140, but SPIRV-Cross's UBO support for desktop GLSL 140 seems to be broken: Mesa throws the error "interface blocks with an instance name are not allowed in GLSL 1.40 (GLSL 1.50 or GLSL ES 3.00 required)" when trying to compile a desktop GLSL 140 shader created by SPIRV-Cross that uses UBOs
            have_binding_support = version >= 130;
            break;
        case Dialect::GLSL_WITHOUT_420PACK:
            have_uniform_buffer_support = version >= 150; // Actually supported starting from GLSL 140, but SPIRV-Cross's UBO support for desktop GLSL 140 seems to be broken: Mesa throws the error "interface blocks with an instance name are not allowed in GLSL 1.40 (GLSL 1.50 or GLSL ES 3.00 required)" when trying to compile a desktop GLSL 140 shader created by SPIRV-Cross that uses UBOs
            have_binding_support = version >= 420;
            break;
        case Dialect::GLSL_ES:
            have_uniform_buffer_support = version >= 300;
            have_binding_support = version >= 310;
            break;
    }

    spvc_parsed_ir ir;
    CHECK(spvc_context_parse_spirv((spvc_context)context, (const SpvId *)spirv_input, spirv_input_size, &ir));

    spvc_compiler compiler;
    CHECK(spvc_context_create_compiler((spvc_context)context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    spvc_resources resources;
    CHECK(spvc_compiler_create_shader_resources(compiler, &resources));

    // In some old GLSL versions, the names of the outputs for a stage must exactly match the names of the inputs for the following stage,
    // so we mangle the names of inputs and outputs here to be based on their locations to ensure that they match.
    SpvExecutionModel execution_model = spvc_compiler_get_execution_model(compiler);
    if (execution_model != SpvExecutionModelVertex) {
        std::string stage_input_cookie_string;
        if (std::isdigit(stage_input_cookie[0])) {
            stage_input_cookie_string = "_";
        }
        stage_input_cookie_string += stage_input_cookie;
        stage_input_cookie_string += "_varying";
        const spvc_reflected_resource *input_list;
        size_t input_list_size;
        CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &input_list, &input_list_size));
        for (size_t i = 0; i < input_list_size; ++i) {
            spvc_compiler_set_name(compiler, input_list[i].id, (stage_input_cookie_string + std::to_string(spvc_compiler_get_decoration(compiler, input_list[i].id, SpvDecorationLocation))).c_str());
        }
    }
    if (execution_model != SpvExecutionModelFragment) {
        std::string stage_output_cookie_string;
        if (std::isdigit(stage_output_cookie[0])) {
            stage_output_cookie_string = "_";
        }
        stage_output_cookie_string += stage_output_cookie;
        stage_output_cookie_string += "_varying";
        const spvc_reflected_resource *output_list;
        size_t output_list_size;
        CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &output_list, &output_list_size));
        for (size_t i = 0; i < output_list_size; ++i) {
            spvc_compiler_set_name(compiler, output_list[i].id, (stage_output_cookie_string + std::to_string(spvc_compiler_get_decoration(compiler, output_list[i].id, SpvDecorationLocation))).c_str());
        }
    }

    const spvc_reflected_resource *uniform_buffer_list;
    size_t uniform_buffer_list_size;
    CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &uniform_buffer_list, &uniform_buffer_list_size));

    if (have_uniform_buffer_support) {
        if (!have_binding_support && uniform_buffer_list_size > 1) {
            error_message = "This shader requires more than one uniform buffer, but this is not supported by your graphics driver";
            return false;
        }
        for (uint32_t uniform_buffer_index = 0; uniform_buffer_index < (uint32_t)uniform_buffer_list_size; ++uniform_buffer_index) {
            spvc_compiler_set_decoration(compiler, uniform_buffer_list[uniform_buffer_index].id, SpvDecorationBinding, uniform_buffer_index);
        }
    }

    const spvc_reflected_resource *texture_list;
    size_t texture_list_size;
    CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &texture_list, &texture_list_size));
    for (uint32_t texture_index = 0; texture_index < (uint32_t)texture_list_size; ++texture_index) {
        spvc_compiler_set_decoration(compiler, texture_list[texture_index].id, SpvDecorationBinding, texture_index);
    }

    spvc_compiler_options options;
    CHECK(spvc_compiler_create_compiler_options(compiler, &options));
    CHECK(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, version));
    CHECK(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, dialect == Dialect::GLSL_ES ? SPVC_TRUE : SPVC_FALSE));
    CHECK(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, dialect == Dialect::GLSL_WITH_420PACK && have_binding_support));
    CHECK(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_EMIT_UNIFORM_BUFFER_AS_PLAIN_UNIFORMS, !have_uniform_buffer_support));
    CHECK(spvc_compiler_install_compiler_options(compiler, options));

    const char *glsl;
    CHECK(spvc_compiler_compile(compiler, &glsl));

    glsl_output = glsl;
    spvc_context_release_allocations((spvc_context)context);
    return true;
}
