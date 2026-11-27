/*
** spirv/retarget-spirv-version.h
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

#ifndef SPIRV_RETARGET_SPIRV_VERSION_H
#define SPIRV_RETARGET_SPIRV_VERSION_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mkxp_spirv_utils {
    /*
     * Changes the version of a SPIR-V module.
     *
     * You must check the validity of the input SPIR-V module yourself before passing it to this function; it's undefined behavior to pass an invalid SPIR-V module to this function.
     *
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in 32-bit words.
     * target_version: The SPIR-V version to change it to, as the 32-bit word that appears in the SPIR-V header in that SPIR-V version. For example, 0x00010600 for SPIR-V 1.6.
     * allow_version_upgrade: If true, allows the SPIR-V version of the output SPIR-V module to be greater than that of the input SPIR-V module. If false, the output SPIR-V module's version will be at most the input SPIR-V module's version.
     * allow_version_downgrade: If true, allows the SPIR-V version of the output SPIR-V module to be less than that of the input SPIR-V module. If false, the output SPIR-V module's version will be at least the input SPIR-V module's version.
     * spirv_output: If the transform is successful, this will be set to the transformed SPIR-V output. Otherwise this will be unchanged.
     * error_message: If the transform is successful, this will be unchanged. Otherwise, this will be set to a string containing the error that occurred.
     *
     * Return value: `true` if the transform was successful, otherwise `false`.
     */
    bool retarget_spirv_version(
        const uint32_t *spirv_input,
        size_t spirv_input_size,
        uint32_t target_version,
        bool allow_version_upgrade,
        bool allow_version_downgrade,
        std::vector<uint32_t> &spirv_output,
        std::string &error_message
    );
}

#endif // SPIRV_RETARGET_SPIRV_VERSION_H
