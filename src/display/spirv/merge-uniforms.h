/*
** spirv/merge-uniforms.h
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

#ifndef SPIRV_MERGE_UNIFORMS_H
#define SPIRV_MERGE_UNIFORMS_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "spirv.h"

namespace mkxp_spirv_utils {
    namespace merge_uniforms_detail {
        struct Uniform
        {
            // The type of this uniform.
            SpirvType type;
            // If this uniform is a matrix or array of matrices,
            // this is true if the uniform is declared as row-major in the SPIR-V module, or false otherwise.
            // Might be different from whether or not the matrix is actually row-major in memory, since the graphics API might not support one of these two matrix memory layouts.
            bool is_logically_row_major;
            // If this uniform is a matrix or array of matrices,
            // this is the matrix stride (i.e. the number of bytes between adjacent rows or columns, depending on if this matrix is row-major or column-major) of this uniform as declared in the SPIR-V module.
            // Might be different from the physical matrix stride of this uniform in memory, since the graphics API might have alignment rules that need to be met.
            uint32_t logical_matrix_stride;
            // If this uniform is an array,
            // this is the array stride (i.e. the number of bytes between adjacent elements of the array) of this uniform as declared in the SPIR-V module.
            // If this uniform is a multidimensional array, this is the most significant array stride.
            // Might be different from the physical array stride of this uniform in memory, since the graphics API might have alignment rules that need to be met.
            uint32_t logical_array_stride;

            bool operator==(const Uniform &other) const noexcept;
            bool operator!=(const Uniform &other) const noexcept;
        };

        struct UniformTrieNode
        {
            // Maps an OpAccessChain index to the next node.
            std::unordered_map<uint32_t, std::unique_ptr<UniformTrieNode>> children;
            // If `children` is empty, this is the name of the uniform that this node refers to.
            // Otherwise, this is empty.
            std::string uniform_name;
        };
    };

    /*
     * Rewrites SPIR-V modules to make them all have the same uniforms.
     *
     * You must check the validity of the input SPIR-V module yourself before passing it to this function; it's undefined behavior to pass an invalid SPIR-V module to this function.
     *
     * spirv_input: Pointer to the input SPIR-V bytecode.
     * spirv_input_size: Length of the input SPIR-V bytecode in 32-bit words.
     * uniforms: A list of uniforms that appear in at least one SPIR-V module in the entire program. All of the uniform buffers in this SPIR-V module will be replaced with one uniform buffer with these uniforms, in the order given in this list, as members, so you may want to sort the uniforms in such a way that minimizes the size of this uniform buffer.
     * uniform_id_map: For each uniform that appears in this SPIR-V module, this is a mapping from the name of the uniform to the SPIR-V ID for its type.
     * uniform_trie: The root node of this trie contains one child for each uniform buffer in this SPIR-V module, keyed by the SPIR-V ID of the uniform buffer. The children of the root node are tries that contain the OpAccessChain index chain to reach each uniform in that uniform buffer. The leaf nodes contain the names of the uniforms.
     * spirv_output: If the transform is successful, this will be set to the transformed SPIR-V output. Otherwise this will be unchanged.
     * error_message: If the transform is successful, this will be unchanged. Otherwise, this will be set to a string containing the error that occurred.
     *
     * Return value: `true` if the transform was successful, otherwise `false`.
     */
    bool merge_uniforms(
        const uint32_t *spirv_input,
        size_t spirv_input_size,
        const std::vector<std::pair<std::string, merge_uniforms_detail::Uniform>> &uniforms,
        const std::unordered_map<std::string, uint32_t> &uniform_type_map,
        const merge_uniforms_detail::UniformTrieNode &uniform_trie,
        std::vector<uint32_t> &spirv_output,
        std::string &error_message
    );
}

#endif // SPIRV_MERGE_UNIFORMS_H
