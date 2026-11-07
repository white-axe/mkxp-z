/*
 ** spirv-optimizer.cpp
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

#include "spirv-optimizer.h"

#include <spirv-tools/libspirv.h>

static spv_target_env get_target_env(SpirvOptimizer::Target target) {
    switch (target) {
        case SpirvOptimizer::Target::SPIRV_1_0: return SPV_ENV_UNIVERSAL_1_0;
        case SpirvOptimizer::Target::SPIRV_1_1: return SPV_ENV_UNIVERSAL_1_1;
        case SpirvOptimizer::Target::SPIRV_1_2: return SPV_ENV_UNIVERSAL_1_2;
        case SpirvOptimizer::Target::SPIRV_1_3: return SPV_ENV_UNIVERSAL_1_3;
        case SpirvOptimizer::Target::SPIRV_1_4: return SPV_ENV_UNIVERSAL_1_4;
        case SpirvOptimizer::Target::SPIRV_1_5: return SPV_ENV_UNIVERSAL_1_5;
        case SpirvOptimizer::Target::SPIRV_1_6: return SPV_ENV_UNIVERSAL_1_6;
    }
}

SpirvOptimizer::SpirvOptimizer(Target target) : optimizer(spvOptimizerCreate(get_target_env(target))), validator_options(spvValidatorOptionsCreate()), optimizer_options(spvOptimizerOptionsCreate()) {
    spvOptimizerRegisterLegalizationPasses((spv_optimizer_t *)optimizer);
    spvOptimizerRegisterPerformancePasses((spv_optimizer_t *)optimizer);
    spvValidatorOptionsSetBeforeHlslLegalization((spv_validator_options_t *)validator_options, true);
    spvOptimizerOptionsSetValidatorOptions((spv_optimizer_options_t *)optimizer_options, (spv_validator_options_t *)validator_options);
    spvOptimizerOptionsSetRunValidator((spv_optimizer_options_t *)optimizer_options, true);
}

SpirvOptimizer::~SpirvOptimizer() {
    if (optimizer_options != nullptr) {
        spvOptimizerOptionsDestroy((spv_optimizer_options_t *)optimizer_options);
    }
    if (validator_options != nullptr) {
        spvValidatorOptionsDestroy((spv_validator_options_t *)validator_options);
    }
    if (optimizer != nullptr) {
        spvOptimizerDestroy((spv_optimizer_t *)optimizer);
    }
}

SpirvOptimizer::SpirvOptimizer(SpirvOptimizer &&other) noexcept {
    *this = std::move(other);
}

SpirvOptimizer &SpirvOptimizer::operator=(SpirvOptimizer &&other) noexcept {
    optimizer = other.optimizer;
    other.optimizer = nullptr;
    validator_options = other.validator_options;
    other.validator_options = nullptr;
    optimizer_options = other.optimizer_options;
    other.optimizer_options = nullptr;
    return *this;
}

bool SpirvOptimizer::optimize(std::vector<uint8_t> &spirv_output, const uint8_t *spirv_input, size_t spirv_input_size) {
    assert((uintptr_t)spirv_input % (uintptr_t)alignof(uint32_t) == 0); // Should be true because memory allocated by malloc() should be sufficiently aligned for uint32_t
    if (spirv_input_size % 4 != 0) {
        return false;
    }
    spv_binary_t *binary;
    if (spvOptimizerRun((spv_optimizer_t *)optimizer, (const uint32_t *)spirv_input, spirv_input_size / 4, &binary, (spv_optimizer_options_t *)optimizer_options) != SPV_SUCCESS) {
        return false;
    }
    spirv_output.clear();
    const uint8_t *spirv = (const uint8_t *)binary->code;
    spirv_output = std::vector<uint8_t>(spirv, spirv + (size_t)4 * binary->wordCount);
    spvBinaryDestroy(binary);
    return true;
}
