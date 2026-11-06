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

#define CHECK(result) do { \
    if ((result) != SPVC_SUCCESS) { \
        error_message = spvc_context_get_last_error_string((spvc_context)context); \
        spvc_context_release_allocations((spvc_context)context); \
        return false; \
    } \
} while (0)

bool SpirvToGlsl::compile(std::string &glsl_output, std::string &error_message, const uint8_t *spirv_input, size_t spirv_input_size, uint16_t version, Dialect dialect) {
    assert((uintptr_t)spirv_input % (uintptr_t)alignof(uint32_t) == 0); // Should be true because memory allocated by malloc() should be sufficiently aligned for uint32_t
    if (spirv_input_size % 4 != 0) {
        error_message = "SPIR-V input size must be a multiple of 4";
        return false;
    }

    spvc_parsed_ir ir;
    CHECK(spvc_context_parse_spirv((spvc_context)context, (const SpvId *)spirv_input, spirv_input_size / 4, &ir));

    spvc_compiler compiler;
    CHECK(spvc_context_create_compiler((spvc_context)context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));

    spvc_compiler_options options;
    CHECK(spvc_compiler_create_compiler_options(compiler, &options));
    CHECK(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, version));
    CHECK(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, dialect == Dialect::GLSL_ES ? SPVC_TRUE : SPVC_FALSE));
    CHECK(spvc_compiler_install_compiler_options(compiler, options));

    const char *output;
    CHECK(spvc_compiler_compile(compiler, &output));

    glsl_output = output;
    spvc_context_release_allocations((spvc_context)context);
    return true;
}
