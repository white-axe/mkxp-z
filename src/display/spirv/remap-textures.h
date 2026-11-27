/*
** spirv/remap-textures.h
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

#ifndef SPIRV_REMAP_TEXTURES_H
#define SPIRV_REMAP_TEXTURES_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "spirv.h"

namespace mkxp_spirv_utils {
    /*
     * Remaps the textures in a SPIR-V module to have sequential texture units, in order to eliminate issues where two textures are assigned the same texture unit.
     *
     * Additionally, certain old GLSL versions don't support explicitly specifying texture units for textures,
     * and simply assign the texture units sequentially in the order that the textures are declared in the GLSL source code.
     * Therefore, we also make sure every texture used in at least one shader is declared in every shader,
     * so that there are no "gaps" in the texture units in any of the shaders if we're compiling from SPIR-V to one of these old GLSL versions.
     *
     * You must check the validity of the input SPIR-V module yourself before passing it to this function; it's undefined behavior to pass an invalid SPIR-V module to this function.
     *
     * Assumes the input SPIR-V version is at least SPIR-V 1.4.
     * It's not undefined behavior to pass a SPIR-V module with an earlier SPIR-V version to this function, but the output SPIR-V module might be invalid in this case.
     *
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in 32-bit words.
     * textures: A list of textures that appear in at least one SPIR-V module in the entire program. The texture units will be assigned sequentially, starting from texture unit 0, in the order given in this list.
     * texture_id_map: For each texture that appears in this SPIR-V module, this is a mapping from the name of the texture to its SPIR-V ID.
     * spirv_output: If the transform is successful, this will be set to the transformed SPIR-V output. Otherwise this will be unchanged.
     * error_message: If the transform is successful, this will be unchanged. Otherwise, this will be set to a string containing the error that occurred.
     *
     * Return value: `true` if the transform was successful, otherwise `false`.
     */
    bool remap_textures(
        const uint32_t *spirv_input,
        size_t spirv_input_size,
        const std::vector<std::pair<std::string, SpirvTexture::Type>> &textures,
        const std::unordered_map<std::string, uint32_t> &texture_id_map,
        std::vector<uint32_t> &spirv_output,
        std::string &error_message
    );
}

#endif // SPIRV_REMAP_TEXTURES_H
