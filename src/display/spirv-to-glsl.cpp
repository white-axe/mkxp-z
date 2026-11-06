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
#include <cstring>
#include <iterator>
#include <sstream>
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

SpirvToGlsl::SpirvToGlsl(SpirvToGlsl &&other) noexcept {
    *this = std::move(other);
}

SpirvToGlsl &SpirvToGlsl::operator=(SpirvToGlsl &&other) noexcept {
    context = other.context;
    other.context = nullptr;
    return *this;
}

static std::string get_struct_member_name(const std::vector<const char *> &struct_name_stack, const char *struct_member_name) {
    std::stringstream stringstream;
    std::copy(struct_name_stack.begin(), struct_name_stack.end(), std::ostream_iterator<const char *>(stringstream, "."));
    stringstream << (struct_member_name != nullptr ? struct_member_name : "");
    return stringstream.str();
}

#define CHECK(result) do { \
    if ((result) != SPVC_SUCCESS) { \
        error_message = spvc_context_get_last_error_string((spvc_context)context); \
        spvc_context_release_allocations((spvc_context)context); \
        return false; \
    } \
} while (0)

bool SpirvToGlsl::compile(std::string &glsl_output, std::vector<Ubo> &ubos_output, std::unordered_map<std::string, Uniform> &uniforms_output, std::string &error_message, const uint8_t *spirv_input, size_t spirv_input_size, uint16_t version, Dialect dialect) {
    assert((uintptr_t)spirv_input % (uintptr_t)alignof(uint32_t) == 0); // Should be true because memory allocated by malloc() should be sufficiently aligned for uint32_t
    if (spirv_input_size % 4 != 0) {
        error_message = "SPIR-V input size must be a multiple of 4";
        return false;
    }

    bool have_ubo_support;
    bool have_binding_support;
    switch (dialect) {
        case Dialect::GLSL_WITH_420PACK:
            have_ubo_support = version >= 140;
            have_binding_support = version >= 130;
            break;
        case Dialect::GLSL_WITHOUT_420PACK:
            have_ubo_support = version >= 140;
            have_binding_support = version >= 420;
            break;
        case Dialect::GLSL_ES:
            have_ubo_support = version >= 300;
            have_binding_support = version >= 310;
    }

    spvc_parsed_ir ir;
    CHECK(spvc_context_parse_spirv((spvc_context)context, (const SpvId *)spirv_input, spirv_input_size / 4, &ir));

    spvc_compiler compiler;
    CHECK(spvc_context_create_compiler((spvc_context)context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    spvc_resources resources;
    CHECK(spvc_compiler_create_shader_resources(compiler, &resources));

    const spvc_reflected_resource *ubo_list;
    size_t ubo_list_size;
    CHECK(spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &ubo_list, &ubo_list_size));

    std::vector<Ubo> ubos;
    if (have_ubo_support) {
        if (!have_binding_support && ubo_list_size > 1) {
            error_message = "This shader requires more than one uniform buffer, but this is not supported by your graphics driver";
            return false;
        }
        for (uint32_t ubo_index = 0; ubo_index < (uint32_t)ubo_list_size; ++ubo_index) {
            spvc_type struct_type = spvc_compiler_get_type_handle(compiler, ubo_list[ubo_index].base_type_id);
            size_t size;
            CHECK(spvc_compiler_get_declared_struct_size(compiler, struct_type, &size));
            ubos.push_back({
                size,
                have_binding_support ? spvc_compiler_get_decoration(compiler, ubo_list[ubo_index].id, SpvDecorationBinding) : 0,
            });
        }
    }

    static const char *const default_uniform_buffer_names[] = {
        "$Global", // Default name of the default uniform buffer used by glslang's HLSL frontend
        "gl_DefaultUniformBlock", // Default name of the default uniform buffer used by glslang's GLSL frontend
    };
    bool default_uniform_buffer_seen = false;
    std::unordered_map<std::string, Uniform> uniforms;
    std::vector<const char *> struct_name_stack;
    struct UniformNode
    {
        const char *name;
        spvc_type_id type_id;
        size_t offset;
        size_t member_index;
    };
    std::vector<UniformNode> uniform_stack;
    for (uint32_t ubo_index = 0; ubo_index < (uint32_t)ubo_list_size; ++ubo_index) {
        const char *ubo_name = spvc_compiler_get_name(compiler, ubo_list[ubo_index].base_type_id);
        if (!default_uniform_buffer_seen) {
            for (size_t j = 0; j < sizeof default_uniform_buffer_names / sizeof *default_uniform_buffer_names; ++j) {
                if (std::strcmp(ubo_name, default_uniform_buffer_names[j]) == 0) {
                    default_uniform_buffer_seen = true;
                    ubo_name = nullptr;
                    break;
                }
            }
        }
        uniform_stack.emplace_back(UniformNode {
            ubo_name,
            ubo_list[ubo_index].base_type_id,
            0,
            0,
        });
        while (!uniform_stack.empty()) {
            UniformNode &uniform = uniform_stack.back();
            spvc_type type = spvc_compiler_get_type_handle(compiler, uniform.type_id);
            spvc_basetype basetype = spvc_type_get_basetype(type);
            if (basetype == SPVC_BASETYPE_STRUCT) {
                if (uniform.member_index < (size_t)spvc_type_get_num_member_types(type)) {
                    unsigned int offset;
                    size_t member_index = uniform.member_index;
                    CHECK(spvc_compiler_type_struct_member_offset(compiler, type, uniform.member_index++, &offset));
                    if (member_index == 0 && uniform.name != nullptr) {
                        struct_name_stack.emplace_back(uniform.name);
                    }
                    uniform_stack.emplace_back(UniformNode {
                        spvc_compiler_get_member_name(compiler, uniform.type_id, member_index),
                        spvc_type_get_member_type(type, member_index),
                        uniform.offset + (size_t)offset,
                        0,
                    });
                } else {
                    if (uniform.member_index > 0 && uniform.name != nullptr) {
                        assert(struct_name_stack.back() == uniform.name);
                        struct_name_stack.pop_back();
                    }
                    uniform_stack.pop_back();
                }
            } else {
                Uniform::Type uniform_type;
                switch (basetype) {
                    case SPVC_BASETYPE_INT8: uniform_type = Uniform::Type::INT8; break;
                    case SPVC_BASETYPE_UINT8: uniform_type = Uniform::Type::UINT8; break;
                    case SPVC_BASETYPE_INT16: uniform_type = Uniform::Type::INT16; break;
                    case SPVC_BASETYPE_UINT16: uniform_type = Uniform::Type::UINT16; break;
                    case SPVC_BASETYPE_INT32: uniform_type = Uniform::Type::INT32; break;
                    case SPVC_BASETYPE_UINT32: uniform_type = Uniform::Type::UINT32; break;
                    case SPVC_BASETYPE_INT64: uniform_type = Uniform::Type::INT64; break;
                    case SPVC_BASETYPE_UINT64: uniform_type = Uniform::Type::UINT64; break;
                    case SPVC_BASETYPE_FP16: uniform_type = Uniform::Type::FP16; break;
                    case SPVC_BASETYPE_FP32: uniform_type = Uniform::Type::FP32; break;
                    case SPVC_BASETYPE_FP64: uniform_type = Uniform::Type::FP64; break;
                    default: uniform_type = Uniform::Type::UNKNOWN; break;
                }
                uniforms.emplace(
                    get_struct_member_name(struct_name_stack, uniform.name),
                    have_ubo_support
                        ? Uniform {
                            uniform_type,
                            Uniform::Flavor::UBO,
                            ubo_index,
                            uniform.offset,
                        }
                        : Uniform {
                            uniform_type,
                            Uniform::Flavor::PLAIN,
                            (uint32_t)uniforms.size(),
                            0,
                        }
                );
                uniform_stack.pop_back();
            }
        }
    }

    spvc_compiler_options options;
    CHECK(spvc_compiler_create_compiler_options(compiler, &options));
    CHECK(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, version));
    CHECK(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, dialect == Dialect::GLSL_ES ? SPVC_TRUE : SPVC_FALSE));
    CHECK(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, dialect == Dialect::GLSL_WITH_420PACK && have_binding_support));
    CHECK(spvc_compiler_install_compiler_options(compiler, options));

    const char *glsl;
    CHECK(spvc_compiler_compile(compiler, &glsl));

    glsl_output = glsl;
    ubos_output = std::move(ubos);
    uniforms_output = std::move(uniforms);
    spvc_context_release_allocations((spvc_context)context);
    return true;
}
