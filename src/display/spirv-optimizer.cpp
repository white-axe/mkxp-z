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

#include "exception.h"
#include "spirv-optimizer.h"

#include <spirv-tools/libspirv.h>

SpirvOptimizer::SpirvOptimizer(Type type) : optimizer(spvOptimizerCreate(SPV_ENV_UNIVERSAL_1_6)), validator_options(spvValidatorOptionsCreate()), optimizer_options(spvOptimizerOptionsCreate()) {
    if (optimizer == nullptr || validator_options == nullptr || optimizer_options == nullptr) {
        if (optimizer_options != nullptr) {
            spvOptimizerOptionsDestroy((spv_optimizer_options_t *)optimizer_options);
        }
        if (validator_options != nullptr) {
            spvValidatorOptionsDestroy((spv_validator_options_t *)validator_options);
        }
        if (optimizer != nullptr) {
            spvOptimizerDestroy((spv_optimizer_t *)optimizer);
        }
        throw Exception(Exception::MKXPError, "Failed to initialize SPIR-V Tools optimizer");
    }

    switch (type) {
        case Type::LEGALIZATION:
            spvOptimizerRegisterLegalizationPasses((spv_optimizer_t *)optimizer);
            spvValidatorOptionsSetBeforeHlslLegalization((spv_validator_options_t *)validator_options, true);
            break;
        case Type::PERFORMANCE:
            spvOptimizerRegisterPerformancePasses((spv_optimizer_t *)optimizer);
            break;
    }

    spvOptimizerOptionsSetValidatorOptions((spv_optimizer_options_t *)optimizer_options, (spv_validator_options_t *)validator_options);
    spvOptimizerOptionsSetRunValidator((spv_optimizer_options_t *)optimizer_options, type == Type::LEGALIZATION); // TODO: always enable validation
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

SpirvOptimizer::SpirvOptimizer(SpirvOptimizer &&other) noexcept : optimizer(other.optimizer), validator_options(other.validator_options), optimizer_options(other.optimizer_options) {
    other.optimizer = nullptr;
    other.validator_options = nullptr;
    other.optimizer_options = nullptr;
}

SpirvOptimizer &SpirvOptimizer::operator=(SpirvOptimizer &&other) noexcept {
    this->~SpirvOptimizer();
    new(this) SpirvOptimizer(std::move(other));
    return *this;
}

static thread_local std::string *consumer_sink;

static void consumer(spv_message_level_t level, const char *source, const spv_position_t *position, const char *message) {
    *consumer_sink = message;
}

bool SpirvOptimizer::transform(
    const uint32_t *spirv_input,
    size_t spirv_input_size,
    std::vector<uint32_t> &spirv_output,
    std::string &error_message
) {
    std::string error;
    consumer_sink = &error;
    spvOptimizerSetMessageConsumer((spv_optimizer_t *)optimizer, consumer);
    spv_binary_t *binary;
    if (spvOptimizerRun((spv_optimizer_t *)optimizer, (const uint32_t *)spirv_input, spirv_input_size, &binary, (spv_optimizer_options_t *)optimizer_options) != SPV_SUCCESS) {
        error_message = std::move(error);
        return false;
    }
    spirv_output.clear();
    const uint32_t *spirv = (const uint32_t *)binary->code;
    spirv_output = std::vector<uint32_t>(spirv, spirv + binary->wordCount);
    spvBinaryDestroy(binary);
    return true;
}
