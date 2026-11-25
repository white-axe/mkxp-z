/*
** spirv-validator.h
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

#ifndef SPIRV_VALIDATOR_H
#define SPIRV_VALIDATOR_H

#include <cstddef>
#include <cstdint>
#include <string>

class SpirvValidator
{
private:
    void *context;

public:
    SpirvValidator();
    ~SpirvValidator();
    SpirvValidator(const SpirvValidator &other) = delete;
    SpirvValidator(SpirvValidator &&other) noexcept;
    SpirvValidator &operator=(const SpirvValidator &other) = delete;
    SpirvValidator &operator=(SpirvValidator &&other) noexcept;

    /*
     * Validates SPIR-V bytecode.
     *
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in 32-bit words.
     * error_message: If validation is successful, this will be unchanged. Otherwise, this will be set to a string containing the error that occurred.
     *
     * Return value: `true` if validation was successful, otherwise `false`.
     */
    bool validate(
        const uint32_t *spirv_input,
        size_t spirv_input_size,
        std::string &error_message
    );
};

#endif // SPIRV_VALIDATOR_H
