/*
 ** spirv/merge-uniforms.cpp
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

#include <map>

#include "spirv-transform.h"
#include "spirv/merge-uniforms.h"

#include <spirv/unified1/spirv.h>

static uint32_t get_scalar_type_logical_size(SpirvType::ScalarType scalar_type) noexcept {
    switch (scalar_type) {
        case SpirvType::INT8:
        case SpirvType::UINT8:
            return 1;
        case SpirvType::INT16:
        case SpirvType::UINT16:
        case SpirvType::FP16:
            return 2;
        case SpirvType::UNKNOWN:
        case SpirvType::INT32:
        case SpirvType::UINT32:
        case SpirvType::FP32:
            return 4;
        case SpirvType::INT64:
        case SpirvType::UINT64:
        case SpirvType::FP64:
            return 8;
    }
}

bool mkxp_spirv_utils::merge_uniforms_detail::Uniform::operator==(const Uniform &other) const noexcept {
    return (
        type.scalar_type == other.type.scalar_type
            && type.vector_size == other.type.vector_size
            && type.columns == other.type.columns
            && type.array_dimensions == other.type.array_dimensions
            && is_logically_row_major == other.is_logically_row_major
            && logical_matrix_stride == other.logical_matrix_stride
            && logical_array_stride == other.logical_array_stride
    );
}

bool mkxp_spirv_utils::merge_uniforms_detail::Uniform::operator!=(const Uniform &other) const noexcept {
    return !(*this == other);
}

bool mkxp_spirv_utils::merge_uniforms(
    const uint32_t *spirv_input,
    size_t spirv_input_size,
    const std::vector<std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform>> &uniforms,
    const std::unordered_map<std::string, uint32_t> &uniform_type_map,
    const mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode &uniform_trie,
    std::vector<uint32_t> &spirv_output,
    std::string &error_message
) {
    std::unordered_map<std::string, uint32_t> uniform_index_map;
    for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
        uniform_index_map.emplace(uniforms[uniform_index].first, uniform_index);
    }

    class MergeUniformsTransform : public SpirvTransform
    {
    private:
        const std::vector<std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform>> &uniforms;
        const std::unordered_map<std::string, uint32_t> &uniform_type_map;
        const std::unordered_map<std::string, uint32_t> &uniform_index_map;
        const mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode &uniform_trie;

        // SPIR-V ID for our merged uniform buffer.
        uint32_t merged_uniform_buffer_id;
        // SPIR-V ID for the struct type for our merged uniform buffer.
        uint32_t merged_uniform_buffer_type_id;
        // SPIR-V ID for a pointer type that points to the struct type for our merged uniform buffer.
        uint32_t merged_uniform_buffer_pointer_type_id;
        // For each uniform in our merged uniform buffer, this contains a SPIR-V ID for a constant used to access the corresponding uniform.
        std::vector<uint32_t> merged_uniform_buffer_index_ids;
        // For each uniform that is an array, this contains the IDs for the array types from least significant to most significant.
        std::vector<std::vector<uint32_t>> uniform_array_type_ids;
        // A map from the SPIR-V IDs of the integer constants defined in the SPIR-V module to the values of those constants.
        std::unordered_map<uint32_t, uint32_t> constants;
        // A map from the SPIR-V IDs for pointer values that refer to some section of a uniform buffer to the uniform trie node for that pointer value.
        std::unordered_map<uint32_t, const mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode &> pointers;
        // A map from SPIR-V types to their SPIR-V IDs. The format of the key is as follows:
        //   * First element: If the type is a matrix, the number of columns. Otherwise, 1.
        //   * Second element: If the type is a matrix, the number of rows. Otherwise, if the type is a vector, the number of components. Otherwise, 1.
        //   * Third element: 0 if the scalar type is an unsigned integer, 1 if the scalar type is a signed integer, 2 if the scalar type is a floating-point value.
        //   * Fourth element: The width of the scalar type in bits.
        std::map<std::tuple<uint8_t, uint8_t, uint8_t, uint32_t>, uint32_t> primitive_types;
        // Reversed version of `primitive_types`.
        std::unordered_map<uint32_t, std::tuple<uint8_t, uint8_t, uint8_t, uint32_t>> primitive_types_reverse;

    public:
        MergeUniformsTransform(
            const std::vector<std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform>> &uniforms,
            const std::unordered_map<std::string, uint32_t> &uniform_type_map,
            const std::unordered_map<std::string, uint32_t> &uniform_index_map,
            const mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode &uniform_trie
        ) :
            uniforms(uniforms),
            uniform_type_map(uniform_type_map),
            uniform_index_map(uniform_index_map),
            uniform_trie(uniform_trie)
        {
        }

    private:
        uint32_t minimum_supported_source_version() const noexcept override {
            return 0x00010400;
        }

        uint32_t maximum_supported_source_version() const noexcept override {
            return 0x00010600;
        }

        bool begin(std::string &error_message) override {
            if (!allocate_id(merged_uniform_buffer_id, error_message)) {
                return false;
            }
            if (!allocate_id(merged_uniform_buffer_type_id, error_message)) {
                return false;
            }
            if (!allocate_id(merged_uniform_buffer_pointer_type_id, error_message)) {
                return false;
            }
            merged_uniform_buffer_index_ids.reserve(uniforms.size());
            uniform_array_type_ids.reserve(uniforms.size());
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                uint32_t id;
                if (!allocate_id(id, error_message)) {
                    return false;
                }
                merged_uniform_buffer_index_ids.push_back(id);
                const std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform> uniform = uniforms[uniform_index];
                std::vector<uint32_t> array_type_ids;
                if (!uniform.second.type.array_dimensions.empty() && !uniform_type_map.count(uniform.first)) {
                    array_type_ids.reserve(uniform.second.type.array_dimensions.size());
                    for (uint32_t dimension_index = 0; dimension_index < uniform.second.type.array_dimensions.size(); ++dimension_index) {
                        if (!allocate_id(id, error_message)) {
                            return false;
                        }
                        array_type_ids.push_back(id);
                    }
                }
                uniform_array_type_ids.emplace_back(std::move(array_type_ids));
            }

            return true;
        }

        bool end_name_debug_section(std::string &error_message) override {
            // Declare the name of our merged uniform buffer
            begin_instruction(SpvOpName);
            push_word(merged_uniform_buffer_id);
            push_string_literal("mkxpz_spirv_sink_uniforms"); // Don't have to worry about name collisions; the SPIR-V to native shading language compiler will deal with it automatically
            end_instruction();

            // Declare the name of our merged uniform buffer's type
            begin_instruction(SpvOpName);
            push_word(merged_uniform_buffer_type_id);
            push_string_literal("mkxpz_spirv_sink_uniforms_t");
            end_instruction();

            // Declare the names of our merged uniform buffer's fields
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                begin_instruction(SpvOpMemberName);
                push_word(merged_uniform_buffer_type_id);
                push_word(uniform_index);
                push_string_literal(uniforms[uniform_index].first.c_str());
                end_instruction();
            }

            return true;
        }

        bool end_process_debug_section(std::string &error_message) override {
            begin_instruction(SpvOpModuleProcessed);
            push_string_literal("mkxp_spirv_utils::merge_uniforms");
            end_instruction();

            return true;
        }

        bool end_annotation_section(std::string &error_message) override {
            // Declare that our merged uniform buffer's type is an interface block
            begin_instruction(SpvOpDecorate);
            push_word(merged_uniform_buffer_type_id);
            push_word(SpvDecorationBlock);
            end_instruction();

            // Declare the linkage of our merged uniform buffer
            begin_instruction(SpvOpDecorate);
            push_word(merged_uniform_buffer_id);
            push_word(SpvDecorationBinding);
            push_word(0);
            end_instruction();
            begin_instruction(SpvOpDecorate);
            push_word(merged_uniform_buffer_id);
            push_word(SpvDecorationDescriptorSet);
            push_word(0);
            end_instruction();

            // Declare the offsets for our merged uniform buffer's fields
            uint32_t offset = 0;
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                const std::pair<std::string, mkxp_spirv_utils::merge_uniforms_detail::Uniform> &uniform = uniforms[uniform_index];

                begin_instruction(SpvOpMemberDecorate);
                push_word(merged_uniform_buffer_type_id);
                push_word(uniform_index);
                push_word(SpvDecorationOffset);
                push_word(offset);
                end_instruction();

                // If it's a matrix or array of matrices, also declare whether it's row-major or column-major and the matrix stride
                if (uniform.second.type.columns != 1) {
                    begin_instruction(SpvOpMemberDecorate);
                    push_word(merged_uniform_buffer_type_id);
                    push_word(uniform_index);
                    push_word(uniform.second.is_logically_row_major ? SpvDecorationRowMajor : SpvDecorationColMajor);
                    end_instruction();

                    begin_instruction(SpvOpMemberDecorate);
                    push_word(merged_uniform_buffer_type_id);
                    push_word(uniform_index);
                    push_word(SpvDecorationMatrixStride);
                    push_word(uniform.second.logical_matrix_stride);
                    end_instruction();
                }

                // If it's an array whose type we need to declare ourselves later on, also declare the array strides
                if (!uniform.second.type.array_dimensions.empty() && !uniform_type_map.count(uniform.first)) {
                    // For multidimensional arrays, we don't really care what the array strides other than the most significant array stride are since
                    // if we need to declare this array type ourselves, it means the shader doesn't read this uniform at all,
                    // so just assign these array strides assuming the elements are tightly packed
                    for (uint32_t dimension_index = 0, array_stride = get_scalar_type_logical_size(uniform.second.type.scalar_type); dimension_index < uniform.second.type.array_dimensions.size() - 1; array_stride *= uniform.second.type.array_dimensions[dimension_index++]) {
                        begin_instruction(SpvOpDecorate);
                        push_word(uniform_array_type_ids[uniform_index][dimension_index]);
                        push_word(SpvDecorationArrayStride);
                        push_word(array_stride);
                        end_instruction();
                    }

                    // The most significant array stride affects the logical size of this uniform, so it's important to get this to be the same in every shader
                    begin_instruction(SpvOpDecorate);
                    push_word(uniform_array_type_ids[uniform_index].back());
                    push_word(SpvDecorationArrayStride);
                    push_word(uniform.second.logical_array_stride);
                    end_instruction();
                }

                // Increase the offset by the logical size of this type
                uint32_t logical_size = !uniform.second.type.array_dimensions.empty()
                    ? uniform.second.type.array_dimensions.back() * uniform.second.logical_array_stride
                    : uniform.second.type.columns != 1
                    ? (uniform.second.is_logically_row_major ? uniform.second.type.vector_size * uniform.second.logical_matrix_stride : uniform.second.type.columns * uniform.second.logical_matrix_stride)
                    : uniform.second.type.vector_size * get_scalar_type_logical_size(uniform.second.type.scalar_type);
                offset += logical_size;
            }

            return true;
        }

        bool end_data_section(std::string &error_message) override {
            // Declare a 32-bit signed integer type if there isn't already one declared, since this is the type of the indices for the members of our merged uniform buffer
            uint32_t int32_type_id;
            {
                std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(1, 1, 1, 32);
                auto it = primitive_types.find(primitive_type);
                if (it != primitive_types.end()) {
                    int32_type_id = it->second;
                } else {
                    if (!allocate_id(int32_type_id, error_message)) {
                        return false;
                    }
                    begin_instruction(SpvOpTypeInt);
                    push_word(int32_type_id);
                    push_word(32);
                    push_word(1);
                    end_instruction();
                    primitive_types.emplace(primitive_type, int32_type_id);
                    primitive_types_reverse.emplace(int32_type_id, primitive_type);
                }
            }

            // Declare a 32-bit unsigned integer type if there isn't already one declared, since this is the type of array dimensions
            uint32_t uint32_type_id;
            {
                std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(1, 1, 0, 32);
                auto it = primitive_types.find(primitive_type);
                if (it != primitive_types.end()) {
                    uint32_type_id = it->second;
                } else {
                    if (!allocate_id(uint32_type_id, error_message)) {
                        return false;
                    }
                    begin_instruction(SpvOpTypeInt);
                    push_word(uint32_type_id);
                    push_word(32);
                    push_word(0);
                    end_instruction();
                    primitive_types.emplace(primitive_type, uint32_type_id);
                    primitive_types_reverse.emplace(uint32_type_id, primitive_type);
                }
            }

            // Declare a constant for each member of our merged uniform buffer
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                begin_instruction(SpvOpConstant);
                push_word(int32_type_id);
                push_word(merged_uniform_buffer_index_ids[uniform_index]);
                push_word(uniform_index);
                end_instruction();
            }

            // Declare the types for any uniforms whose types have not been declared yet
            std::vector<uint32_t> uniform_type_ids;
            uniform_type_ids.reserve(uniforms.size());
            for (uint32_t uniform_index = 0; uniform_index < uniforms.size(); ++uniform_index) {
                {
                    auto it = uniform_type_map.find(uniforms[uniform_index].first);
                    if (it != uniform_type_map.end()) {
                        uniform_type_ids.push_back(it->second);
                        continue;
                    }
                }
                const mkxp_spirv_utils::merge_uniforms_detail::Uniform &uniform = uniforms[uniform_index].second;
                uint8_t scalar_kind;
                switch (uniform.type.scalar_type) {
                    case SpirvType::UNKNOWN:
                    case SpirvType::UINT8:
                    case SpirvType::UINT16:
                    case SpirvType::UINT32:
                    case SpirvType::UINT64:
                        scalar_kind = 0;
                        break;
                    case SpirvType::INT8:
                    case SpirvType::INT16:
                    case SpirvType::INT32:
                    case SpirvType::INT64:
                        scalar_kind = 1;
                        break;
                    case SpirvType::FP16:
                    case SpirvType::FP32:
                    case SpirvType::FP64:
                        scalar_kind = 2;
                        break;
                }
                std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(uniform.type.columns, uniform.type.vector_size, scalar_kind, 8 * get_scalar_type_logical_size(uniform.type.scalar_type));
                uint32_t uniform_type_id;
                auto it = primitive_types.find(primitive_type);
                if (it != primitive_types.end()) {
                    uniform_type_id = it->second;
                } else {
                    if (!allocate_id(uniform_type_id, error_message)) {
                        return false;
                    }
                    begin_instruction(scalar_kind == 2 ? SpvOpTypeFloat : SpvOpTypeInt);
                    push_word(uniform_type_id);
                    push_word(8 * get_scalar_type_logical_size(uniform.type.scalar_type));
                    if (scalar_kind != 2) {
                        push_word(scalar_kind);
                    }
                    end_instruction();
                    if (uniform.type.vector_size != 1) {
                        uint32_t component_type_id = uniform_type_id;
                        if (!allocate_id(uniform_type_id, error_message)) {
                            return false;
                        }
                        begin_instruction(SpvOpTypeVector);
                        push_word(uniform_type_id);
                        push_word(component_type_id);
                        push_word(uniform.type.vector_size);
                        end_instruction();
                    }
                    if (uniform.type.columns != 1) {
                        uint32_t vector_type_id = uniform_type_id;
                        if (!allocate_id(uniform_type_id, error_message)) {
                            return false;
                        }
                        begin_instruction(SpvOpTypeMatrix);
                        push_word(uniform_type_id);
                        push_word(vector_type_id);
                        push_word(uniform.type.columns);
                        end_instruction();
                    }
                }
                if (!uniform.type.array_dimensions.empty()) {
                    for (uint32_t dimension_index = 0; dimension_index < uniform.type.array_dimensions.size(); ++dimension_index) {
                        uint32_t constant_id;
                        if (!allocate_id(constant_id, error_message)) {
                            return false;
                        }
                        begin_instruction(SpvOpConstant);
                        push_word(uint32_type_id);
                        push_word(constant_id);
                        push_word(uniform.type.array_dimensions[dimension_index]);
                        end_instruction();
                        uint32_t element_type_id = uniform_type_id;
                        uniform_type_id = uniform_array_type_ids[uniform_index][dimension_index];
                        begin_instruction(SpvOpTypeArray);
                        push_word(uniform_type_id);
                        push_word(element_type_id);
                        push_word(constant_id);
                        end_instruction();
                    }
                }
                uniform_type_ids.push_back(uniform_type_id);
            }

            // Declare a struct type for our merged uniform buffer
            begin_instruction(SpvOpTypeStruct);
            push_word(merged_uniform_buffer_type_id);
            push_words(uniform_type_ids.begin(), uniform_type_ids.end());
            end_instruction();

            // Declare a pointer type for our merged uniform buffer
            begin_instruction(SpvOpTypePointer);
            push_word(merged_uniform_buffer_pointer_type_id);
            push_word(SpvStorageClassUniform);
            push_word(merged_uniform_buffer_type_id);
            end_instruction();

            // Declare the merged uniform buffer
            begin_instruction(SpvOpVariable);
            push_word(merged_uniform_buffer_pointer_type_id);
            push_word(merged_uniform_buffer_id);
            push_word(SpvStorageClassUniform);
            end_instruction();

            return true;
        }

        bool receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message) override {
            switch (opcode) {
                case SpvOpConstant:
                    constants.emplace(words[1], words[2]);
                    break;

                case SpvOpTypeInt:
                    {
                        std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(1, 1, words[2], words[1]);
                        primitive_types.emplace(primitive_type, words[0]);
                        primitive_types_reverse.emplace(words[0], primitive_type);
                    }
                    break;

                case SpvOpTypeFloat:
                    {
                        std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(1, 1, 2, words[1]);
                        primitive_types.emplace(primitive_type, words[0]);
                        primitive_types_reverse.emplace(words[0], primitive_type);
                    }
                    break;

                case SpvOpTypeVector:
                    {
                        auto it = primitive_types_reverse.find(words[1]);
                        if (it == primitive_types_reverse.end()) {
                            break;
                        }
                        std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(1, words[2], std::get<2>(it->second), std::get<3>(it->second));
                        primitive_types.emplace(primitive_type, words[0]);
                        primitive_types_reverse.emplace(words[0], primitive_type);
                    }
                    break;

                case SpvOpTypeMatrix:
                    {
                        auto it = primitive_types_reverse.find(words[1]);
                        if (it == primitive_types_reverse.end()) {
                            break;
                        }
                        std::tuple<uint8_t, uint8_t, uint8_t, uint32_t> primitive_type = std::make_tuple(words[2], std::get<1>(it->second), std::get<2>(it->second), std::get<3>(it->second));
                        primitive_types.emplace(primitive_type, words[0]);
                        primitive_types_reverse.emplace(words[0], primitive_type);
                    }
                    break;

                default:
                    break;
            }

            switch (opcode) {
                case SpvOpEntryPoint:
                    {
                        // Remove the old uniform buffers from the OpEntryPoint instruction and add our merged uniform buffer to it
                        begin_instruction(opcode);
                        uint16_t word_index = 2;
                        while ((words[word_index] & 0xff000000) != 0) {
                            ++word_index;
                        }
                        ++word_index;
                        push_words(words, words + word_index);
                        for (; word_index < num_words; ++word_index) {
                            uint32_t id = words[word_index];
                            if (!uniform_trie.children.count(id)) {
                                push_word(id);
                            }
                        }
                        push_word(merged_uniform_buffer_id);
                        end_instruction();
                    }
                    break;

                case SpvOpVariable:
                    // Remove the declarations of the old uniform buffers
                    if (!uniform_trie.children.count(words[1])) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpName:
                case SpvOpDecorate:
                case SpvOpDecorateId:
                case SpvOpDecorateString:
                    // Remove debug information and annotations from the old uniform buffers
                    if (!uniform_trie.children.count(words[0])) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpGroupDecorate:
                    // Remove annotations from the old uniform buffers
                    begin_instruction(opcode);
                    push_word(words[0]);
                    for (uint16_t word_index = 1; word_index < num_words; ++word_index) {
                        if (!uniform_trie.children.count(words[word_index])) {
                            push_word(words[word_index]);
                        }
                    }
                    end_instruction();
                    break;

                case SpvOpAccessChain:
                case SpvOpInBoundsAccessChain:
                case SpvOpPtrAccessChain:
                case SpvOpInBoundsPtrAccessChain:
                    {
                        // Rewrite any instructions that try to read from one of the old uniform buffers such that they instead read from the merged uniform buffer
                        uint32_t base = words[2];
                        const mkxp_spirv_utils::merge_uniforms_detail::UniformTrieNode *node;
                        {
                            auto trie_it = uniform_trie.children.find(base);
                            auto pointer_it = pointers.find(base);
                            if (trie_it != uniform_trie.children.end()) {
                                assert(pointer_it == pointers.end());
                                node = trie_it->second.get();
                            } else if (pointer_it != pointers.end()) {
                                assert(trie_it == uniform_trie.children.end());
                                node = &pointer_it->second;
                            } else {
                                // This instruction doesn't try to read one of the old uniform buffers, so pass this instruction through without doing anything
                                begin_instruction(opcode);
                                push_words(words, words + num_words);
                                end_instruction();
                                break;
                            }
                        }
                        // For OpPtrAccessChain and OpInBoundsPtrAccessChain, we just ignore the element operand
                        // since the SPIR-V specification says when reading from a uniform buffer,
                        // it's undefined behavior for the element operand for these two instructions to be nonzero.
                        for (uint16_t word_index = opcode == SpvOpPtrAccessChain || opcode == SpvOpPtrAccessChain ? 4 : 3; word_index < num_words; ++word_index) {
                            uint32_t chain_word = words[word_index];
                            if (node->children.empty()) {
                                push_word(chain_word);
                            } else {
                                uint32_t constant = constants.at(chain_word);
                                node = node->children.at(constant).get();
                                if (node->children.empty()) {
                                    begin_instruction(SpvOpAccessChain);
                                    push_words(words, words + 2);
                                    push_word(merged_uniform_buffer_id);
                                    push_word(merged_uniform_buffer_index_ids[uniform_index_map.at(node->uniform_name)]);
                                }
                            }
                        }
                        if (node->children.empty()) {
                            end_instruction();
                        } else {
                            pointers.emplace(words[1], *node);
                        }
                    }
                    break;

                default:
                    begin_instruction(opcode);
                    push_words(words, words + num_words);
                    end_instruction();
                    break;
            }

            return true;
        }
    };

    return MergeUniformsTransform {
        uniforms,
        uniform_type_map,
        uniform_index_map,
        uniform_trie,
    }.transform(
        spirv_input,
        spirv_input_size,
        spirv_output,
        error_message
    );
}
