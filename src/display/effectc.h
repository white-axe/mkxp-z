/*
** effectc.h
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

#ifndef EFFECTC_H
#define EFFECTC_H

#include "hlsl-to-spirv.h"
#include "spirv-optimizer.h"

namespace effectc {
    /* Compiles a Direct3D 9 effect.
     *
     * error_messages: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing all compilation errors that occurred.
     * shaders_source_code: The HLSL source code for the shaders to be compiled.
     * passes_source_code: The HLSL source code for the passes describing the shaders to be compiled, as one or more Direct3D 9 passes; see <https://github.com/MicrosoftDocs/win32/blob/9bcde4a0658c5b315366da6124466465bb25e5ee/desktop-src/direct3d9/techniques-and-passes.md#passes>.
     * shaders_line_number_offset: Offset to add to the line numbers in error messages in the source code for the shaders. This can be useful if the source code for the shaders contains some kind of preamble.
     *
     * Return value: `true` if compilation succeeded, otherwise `false`.
     */
    bool compile(std::string &error_messages, HlslToSpirv &hlsl_to_spirv_compiler, SpirvOptimizer &optimizer, const char *shaders_source_code, const char *passes_source_code, int shaders_line_number_offset);
}

#endif // EFFECTC_H
