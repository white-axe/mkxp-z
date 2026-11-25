/*
 ** spirv-validator.cpp
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

#include "exception.h"
#include "spirv-validator.h"

#include <spirv-tools/libspirv.h>

SpirvValidator::SpirvValidator() : context(spvContextCreate(SPV_ENV_UNIVERSAL_1_6)) {
    if (context == nullptr) {
        spvContextDestroy((spv_context)context);
        context = nullptr;
        throw Exception(Exception::MKXPError, "Failed to initialize SPIR-V Tools validator");
    }
}

SpirvValidator::~SpirvValidator() {
    if (context != nullptr) {
        spvContextDestroy((spv_context)context);
    }
}

SpirvValidator::SpirvValidator(SpirvValidator &&other) noexcept : context(other.context) {
    other.context = nullptr;
}

SpirvValidator &SpirvValidator::operator=(SpirvValidator &&other) noexcept {
    this->~SpirvValidator();
    new(this) SpirvValidator(std::move(other));
    return *this;
}

bool SpirvValidator::validate(
    const uint32_t *spirv_input,
    size_t spirv_input_size,
    std::string &error_message
) {
    spv_diagnostic diagnostic;
    spv_result_t result = spvValidateBinary(
        (spv_context)context,
        spirv_input,
        spirv_input_size,
        &diagnostic
    );
    if (result != SPV_SUCCESS) {
        error_message.clear();
        error_message = diagnostic->error;
        spvDiagnosticDestroy(diagnostic);
        return false;
    }
    return true;
}
