/*
 ** hlsl-to-spirv.cpp
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

#include <utility>

#include "hlsl-to-spirv.h"

#include "exception.h"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

static glslang_target_language_version_t get_target_language_version(SpirvTarget target) {
    switch (target) {
        case SpirvTarget::SPIRV_1_0: return GLSLANG_TARGET_SPV_1_0;
        case SpirvTarget::SPIRV_1_1: return GLSLANG_TARGET_SPV_1_1;
        case SpirvTarget::SPIRV_1_2: return GLSLANG_TARGET_SPV_1_2;
        case SpirvTarget::SPIRV_1_3: return GLSLANG_TARGET_SPV_1_3;
        case SpirvTarget::SPIRV_1_4: return GLSLANG_TARGET_SPV_1_4;
        case SpirvTarget::SPIRV_1_5: return GLSLANG_TARGET_SPV_1_5;
        case SpirvTarget::SPIRV_1_6: return GLSLANG_TARGET_SPV_1_6;
    }
}

static glslang_stage_t get_stage(SpirvExecutionModel execution_model) {
    switch (execution_model) {
        case SpirvExecutionModel::VERTEX: return GLSLANG_STAGE_VERTEX;
        case SpirvExecutionModel::TESSELLATION_CONTROL: return GLSLANG_STAGE_TESSCONTROL;
        case SpirvExecutionModel::TESSELLATION_EVALUATION: return GLSLANG_STAGE_TESSEVALUATION;
        case SpirvExecutionModel::GEOMETRY: return GLSLANG_STAGE_GEOMETRY;
        case SpirvExecutionModel::FRAGMENT: return GLSLANG_STAGE_FRAGMENT;
    }
}

HlslToSpirv::HlslToSpirv(SpirvTarget target) : target(target), initialized(glslang_initialize_process()) {
    if (!initialized) {
        throw Exception(Exception::MKXPError, "Failed to initialize glslang");
    }
}

HlslToSpirv::~HlslToSpirv() {
    if (initialized) {
        glslang_finalize_process();
    }
}

HlslToSpirv::HlslToSpirv(HlslToSpirv &&other) noexcept : target(other.target), initialized(other.initialized) {
    other.initialized = false;
}

HlslToSpirv &HlslToSpirv::operator=(HlslToSpirv &&other) noexcept {
    this->~HlslToSpirv();
    new(this) HlslToSpirv(std::move(other));
    return *this;
}

extern "C" int mkxp_custom_shaders_set_line_number_offset(int);

bool HlslToSpirv::compile(
    std::vector<uint8_t> &spirv_output,
    std::string &error_messages,
    const char *hlsl_source_code,
    const char *entry_point,
    size_t preamble_size,
        SpirvExecutionModel execution_model,
    Dialect dialect
) {
    // glslang only considers line feeds to be newlines, so we don't count other types of newlines
    size_t preamble_lines = 0;
    for (size_t i = 0; i < preamble_size && hlsl_source_code[i] != 0; ++i) {
        if (hlsl_source_code[i] == '\n') {
            ++preamble_lines;
        }
    }
    mkxp_custom_shaders_set_line_number_offset(-preamble_lines);

    const glslang_input_t input = {
        GLSLANG_SOURCE_HLSL,
        get_stage(execution_model),
        GLSLANG_CLIENT_VULKAN,
        GLSLANG_TARGET_VULKAN_1_4,
        GLSLANG_TARGET_SPV,
        get_target_language_version(target),
        hlsl_source_code,
        0,
        GLSLANG_NO_PROFILE,
        false,
        false,
        (glslang_messages_t)(GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT | GLSLANG_MSG_READ_HLSL_BIT | (dialect == Dialect::D3D9 ? GLSLANG_MSG_HLSL_DX9_COMPATIBLE_BIT : 0)),
        glslang_default_resource(),
        {
            nullptr,
            nullptr,
            nullptr,
        },
        nullptr,
    };
    glslang_shader_t *shader = glslang_shader_create(&input);
    glslang_shader_set_entry_point(shader, entry_point);

    if (!glslang_shader_preprocess(shader, &input)) {
        error_messages = glslang_shader_get_info_log(shader);
        glslang_shader_delete(shader);
        return false;
    }

    if (!glslang_shader_parse(shader, &input)) {
        error_messages = glslang_shader_get_info_log(shader);
        glslang_shader_delete(shader);
        return false;
    }

    glslang_program_t *program = glslang_program_create();
    glslang_program_add_shader(program, shader);
    if (!glslang_program_link(program, input.messages)) {
        error_messages = glslang_program_get_info_log(program);
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return false;
    }

    glslang_program_SPIRV_generate(program, input.stage);
    glslang_shader_delete(shader);
    spirv_output.clear();
    const uint8_t *spirv = (const uint8_t *)glslang_program_SPIRV_get_ptr(program);
    spirv_output = std::vector<uint8_t>(spirv, spirv + sizeof(unsigned int) * glslang_program_SPIRV_get_size(program));
    glslang_program_delete(program);
    return true;
}
