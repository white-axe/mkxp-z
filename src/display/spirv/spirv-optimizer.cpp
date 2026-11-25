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

SpirvOptimizer::SpirvOptimizer(Type type) : optimizer(spvOptimizerCreate(SPV_ENV_UNIVERSAL_1_6)), optimizer_options(spvOptimizerOptionsCreate()) {
    if (optimizer == nullptr || optimizer_options == nullptr) {
        if (optimizer_options != nullptr) {
            spvOptimizerOptionsDestroy((spv_optimizer_options_t *)optimizer_options);
            optimizer_options = nullptr;
        }
        if (optimizer != nullptr) {
            spvOptimizerDestroy((spv_optimizer_t *)optimizer);
            optimizer = nullptr;
        }
        throw Exception(Exception::MKXPError, "Failed to initialize SPIR-V Tools optimizer");
    }

    bool success = false;
    switch (type) {
        case Type::LEGALIZATION:
            {
                static const char *const flags[] = {
                    "--legalize-hlsl",
                };
                success = spvOptimizerRegisterPassesFromFlagsWhilePreservingTheInterface((spv_optimizer_t *)optimizer, (const char **)flags, sizeof flags / sizeof *flags);
            }
            break;
        case Type::PERFORMANCE:
            {
                static const char *const flags[] = {
                    "-O",
                };
                success = spvOptimizerRegisterPassesFromFlagsWhilePreservingTheInterface((spv_optimizer_t *)optimizer, (const char **)flags, sizeof flags / sizeof *flags);
            }
            break;
    }
    if (!success) {
        spvOptimizerOptionsDestroy((spv_optimizer_options_t *)optimizer_options);
        optimizer_options = nullptr;
        spvOptimizerDestroy((spv_optimizer_t *)optimizer);
        optimizer = nullptr;
        throw Exception(Exception::MKXPError, "Failed to initialize SPIR-V Tools optimizer");
    }

    spvOptimizerOptionsSetRunValidator((spv_optimizer_options_t *)optimizer_options, false);
}

SpirvOptimizer::~SpirvOptimizer() {
    if (optimizer_options != nullptr) {
        spvOptimizerOptionsDestroy((spv_optimizer_options_t *)optimizer_options);
    }
    if (optimizer != nullptr) {
        spvOptimizerDestroy((spv_optimizer_t *)optimizer);
    }
}

SpirvOptimizer::SpirvOptimizer(SpirvOptimizer &&other) noexcept : optimizer(other.optimizer), optimizer_options(other.optimizer_options) {
    other.optimizer = nullptr;
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
