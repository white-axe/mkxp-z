/*
 ** spirv-transform.cpp
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

#include <cstring>
#include <utility>

#include "spirv-transform.h"

#include <spirv/unified1/spirv.h>

bool SpirvTransform::transform(
    const uint32_t *input,
    size_t input_size,
    const SpirvType *extra_types,
    const std::vector<uint32_t> *extra_type_array_strides,
    size_t extra_types_size,
    std::vector<uint32_t> &output,
    std::string &error_message
) {
    std::vector<uint32_t> _transformed;
    transformed = &_transformed;
    std::vector<uint32_t> _extra_type_ids;
    extra_type_ids = &_extra_type_ids;
    instruction_index = 0;

    std::vector<std::vector<uint32_t>> raw_extra_type_ids;
    bool seen_source_debug_section = false;
    bool seen_name_debug_section = false;
    bool seen_process_debug_section = false;
    bool seen_annotation_section = false;
    bool seen_data_section = false;
    bool seen_code_section = false;

    if (input_size < 5 || input[0] != SpvMagicNumber) {
        error_message = "Invalid SPIR-V module";
        return false;
    }

    // The newest SPIR-V version that we support right now is SPIR-V 1.6.
    // Newer SPIR-V versions, if any are released,
    // may have additional instructions that are allowed in each section of the SPIR-V module,
    // which would break our detection for where the beginning of each section is,
    // so we need to explicitly check the version.
    if (input[1] > 0x00010600) {
        error_message = "Unsupported SPIR-V version";
        return false;
    }

    _spirv_version = input[1];
    id_bound = input[3];

    if (input[1] > maximum_supported_spirv_version()) {
        error_message = "Unsupported SPIR-V version";
        return false;
    }

    _transformed.insert(_transformed.end(), input, input + 5);

    _extra_type_ids.reserve(extra_types_size);
    raw_extra_type_ids.reserve(extra_types_size);
    for (uint32_t extra_type_index = 0; extra_type_index < extra_types_size; ++extra_type_index) {
        const SpirvType &type = extra_types[extra_type_index];
        uint32_t num_type_ids = 1;
        if (type.vector_size != 1) {
            ++num_type_ids;
        }
        if (type.columns != 1) {
            ++num_type_ids;
        }
        num_type_ids += type.array_dimensions.size();
        std::vector<uint32_t> type_ids;
        type_ids.reserve(num_type_ids);
        for (uint32_t i = 0; i < num_type_ids; ++i) {
            uint32_t type_id;
            if (!allocate_id(type_id, error_message)) {
                return false;
            }
            type_ids.push_back(type_id);
        }
        _extra_type_ids.push_back(type_ids.back());
        raw_extra_type_ids.emplace_back(std::move(type_ids));
    }
    assert_no_instruction();

    if (!begin(error_message)) {
        return false;
    }
    assert_no_instruction();

    for (size_t word_index = 5; word_index < input_size;) {
        uint16_t opcode = input[word_index];
        uint16_t instruction_size = input[word_index] >> 16;
        if (instruction_size == 0 || word_index + (size_t)instruction_size < word_index || word_index + (size_t)instruction_size > input_size) {
            error_message = "Invalid SPIR-V module";
            return false;
        }

        bool is_start_of_source_debug_section = false;
        bool is_start_of_name_debug_section = false;
        bool is_start_of_process_debug_section = false;
        bool is_start_of_annotation_section = false;
        bool is_start_of_data_section = false;
        bool is_start_of_code_section = false;

        // Note: it's intentional that there isn't a break statement at the ends of the cases.
        // We want to make sure that, for example, if we see an annotation instruction without ever having seen a debug instruction,
        // then `is_start_of_debug_section` and `is_start_of_annotation_section` must both be set to `true`.
        switch (opcode) {
            case SpvOpFunction:
                if (!seen_code_section) {
                    seen_code_section = true;
                    is_start_of_code_section = true;
                }

            // Type declarations
            case SpvOpTypeVoid:
            case SpvOpTypeBool:
            case SpvOpTypeInt:
            case SpvOpTypeFloat:
            case SpvOpTypeVector:
            case SpvOpTypeMatrix:
            case SpvOpTypeImage:
            case SpvOpTypeSampler:
            case SpvOpTypeSampledImage:
            case SpvOpTypeArray:
            case SpvOpTypeRuntimeArray:
            case SpvOpTypeStruct:
            case SpvOpTypeOpaque:
            case SpvOpTypePointer:
            case SpvOpTypeFunction:
            case SpvOpTypeEvent:
            case SpvOpTypeDeviceEvent:
            case SpvOpTypeReserveId:
            case SpvOpTypeQueue:
            case SpvOpTypePipe:
            case SpvOpTypeForwardPointer:
            // Constant instructions (i.e. all instructions whose names start with OpConstant or OpSpecConstant)
            case SpvOpConstant:
            case SpvOpConstantComposite:
            case SpvOpConstantSampler:
            case SpvOpConstantNull:
            case SpvOpSpecConstantTrue:
            case SpvOpSpecConstantFalse:
            case SpvOpSpecConstant:
            case SpvOpSpecConstantComposite:
            case SpvOpSpecConstantOp:
            // Global variable declarations
            case SpvOpVariable:
            // The specific four instructions that are mentioned as also being allowed in this section
            case SpvOpUndef:
            case SpvOpLine:
            case SpvOpNoLine:
            case SpvOpExtInst:
                if (!seen_data_section) {
                    seen_data_section = true;
                    is_start_of_data_section = true;
                }

            case SpvOpDecorate:
            case SpvOpMemberDecorate:
            case SpvOpDecorationGroup:
            case SpvOpGroupDecorate:
            case SpvOpGroupMemberDecorate:
            case SpvOpDecorateId:
            case SpvOpMemberDecorateIdEXT:
            case SpvOpDecorateString:
            case SpvOpMemberDecorateString:
                if (!seen_annotation_section) {
                    seen_annotation_section = true;
                    is_start_of_annotation_section = true;
                }

            case SpvOpModuleProcessed:
                if (!seen_process_debug_section) {
                    seen_process_debug_section = true;
                    is_start_of_process_debug_section = true;
                }

            case SpvOpName:
            case SpvOpMemberName:
                if (!seen_name_debug_section) {
                    seen_name_debug_section = true;
                    is_start_of_name_debug_section = true;
                }

            case SpvOpString:
            case SpvOpSourceExtension:
            case SpvOpSource:
            case SpvOpSourceContinued:
                if (!seen_source_debug_section) {
                    seen_source_debug_section = true;
                    is_start_of_source_debug_section = true;
                }

            default:
                break;
        }

        if (is_start_of_source_debug_section) {
            if (!begin_source_debug_section(error_message)) {
                return false;
            }
            assert_no_instruction();
        }

        if (is_start_of_name_debug_section) {
            if (!end_source_debug_section(error_message)) {
                return false;
            }
            assert_no_instruction();

            if (!begin_name_debug_section(error_message)) {
                return false;
            }
            assert_no_instruction();
        }

        if (is_start_of_process_debug_section) {
            if (!end_name_debug_section(error_message)) {
                return false;
            }
            assert_no_instruction();

            if (_spirv_version >= 0x00010100) {
                if (!begin_process_debug_section(error_message)) {
                    return false;
                }
                assert_no_instruction();
            }
        }

        if (is_start_of_annotation_section) {
            if (_spirv_version >= 0x00010100) {
                if (!end_process_debug_section(error_message)) {
                    return false;
                }
                assert_no_instruction();
            }

            // Declare the array strides for each extra type that is an array
            for (uint32_t extra_type_index = 0; extra_type_index < extra_types_size; ++extra_type_index) {
                const SpirvType &type = extra_types[extra_type_index];
                if (!type.array_dimensions.empty()) {
                    const std::vector<uint32_t> &type_ids = raw_extra_type_ids[extra_type_index];
                    for (size_t dimension_index = 0; dimension_index < type.array_dimensions.size(); ++dimension_index) {
                        begin_instruction(SpvOpDecorate);
                        push_word(type_ids[type_ids.size() - dimension_index - 1]);
                        push_word(SpvDecorationArrayStride);
                        push_word(extra_type_array_strides[extra_type_index][dimension_index]);
                        end_instruction();
                    }
                }
            }
            assert_no_instruction();

            if (!begin_annotation_section(error_message)) {
                return false;
            }
            assert_no_instruction();
        }

        if (is_start_of_data_section) {
            if (!end_annotation_section(error_message)) {
                return false;
            }
            assert_no_instruction();

            // Declare each extra type
            for (uint32_t extra_type_index = 0; extra_type_index < extra_types_size; ++extra_type_index) {
                push_type(extra_types[extra_type_index], raw_extra_type_ids[extra_type_index]);
            }
            assert_no_instruction();

            if (!begin_data_section(error_message)) {
                return false;
            }
            assert_no_instruction();
        }

        if (is_start_of_code_section) {
            if (!end_data_section(error_message)) {
                return false;
            }
            assert_no_instruction();

            if (!begin_code_section(error_message)) {
                return false;
            }
            assert_no_instruction();
        }

        if (!receive_instruction(
            opcode,
            input + (word_index + 1),
            instruction_size - 1,
            error_message
        )) {
            return false;
        }
        assert_no_instruction();

        word_index += instruction_size;
    }

    _transformed[3] = id_bound;
    output = std::move(_transformed);
    return true;
}

uint32_t SpirvTransform::maximum_supported_spirv_version() const noexcept {
    return UINT32_MAX;
}

bool SpirvTransform::begin(std::string &error_message) {
    return true;
}

bool SpirvTransform::begin_source_debug_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::end_source_debug_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::begin_name_debug_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::end_name_debug_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::begin_process_debug_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::end_process_debug_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::begin_annotation_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::end_annotation_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::begin_data_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::end_data_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::begin_code_section(std::string &error_message) {
    return true;
}

bool SpirvTransform::receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message) {
    begin_instruction(opcode);
    push_words(words, words + num_words);
    end_instruction();
    return true;
}

uint32_t SpirvTransform::spirv_version() const noexcept {
    return _spirv_version;
}

bool SpirvTransform::allocate_id(uint32_t &id_output, std::string &error_message) {
    if (id_bound == UINT32_MAX) {
        error_message = "Failed to allocate SPIR-V ID";
        return false;
    }
    id_output = id_bound++;
    return true;
}

void SpirvTransform::assert_have_instruction() const noexcept {
    assert(instruction_index != 0);
}

void SpirvTransform::assert_no_instruction() const noexcept {
    assert(instruction_index == 0);
}

void SpirvTransform::push_type(const SpirvType &type, std::vector<uint32_t> &type_ids) {
    if (type.array_dimensions.empty()) {
        uint32_t type_id = type_ids.back();
        type_ids.pop_back();
        if (type.columns == 1) {
            if (type.vector_size == 1) { // Scalar
                switch (type.scalar_type) {
                    case SpirvType::ScalarType::INT8:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(8); // width
                        push_word(1); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::UINT8:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(8); // width
                        push_word(0); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::INT16:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(16); // width
                        push_word(1); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::UINT16:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(16); // width
                        push_word(0); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::INT32:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(32); // width
                        push_word(1); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::UNKNOWN:
                    case SpirvType::ScalarType::UINT32:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(32); // width
                        push_word(0); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::INT64:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(64); // width
                        push_word(1); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::UINT64:
                        begin_instruction(SpvOpTypeInt);
                        push_word(type_id);
                        push_word(64); // width
                        push_word(0); // signedness
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::FP16:
                        begin_instruction(SpvOpTypeFloat);
                        push_word(type_id);
                        push_word(16); // width
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::FP32:
                        begin_instruction(SpvOpTypeFloat);
                        push_word(type_id);
                        push_word(32); // width
                        end_instruction();
                        break;
                    case SpirvType::ScalarType::FP64:
                        begin_instruction(SpvOpTypeFloat);
                        push_word(type_id);
                        push_word(64); // width
                        end_instruction();
                        break;
                }
            } else { // Vector
                uint32_t scalar_type_id = type_ids.back();
                SpirvType scalar_type = type;
                scalar_type.vector_size = 1;
                push_type(scalar_type, type_ids);
                begin_instruction(SpvOpTypeVector);
                push_word(type_id);
                push_word(scalar_type_id);
                push_word(type.vector_size);
                end_instruction();
            }
        } else { // Matrix
            uint32_t vector_type_id = type_ids.back();
            SpirvType vector_type = type;
            vector_type.columns = 1;
            push_type(vector_type, type_ids);
            begin_instruction(SpvOpTypeMatrix);
            push_word(type_id);
            push_word(vector_type_id);
            push_word(type.columns);
            end_instruction();
        }
    } else { // Array
        std::vector<uint32_t> array_type_ids(type_ids.begin() + (type_ids.size() - type.array_dimensions.size()), type_ids.end());
        type_ids.resize(type_ids.size() - type.array_dimensions.size());
        uint32_t element_type_id = type_ids.back();
        SpirvType element_type = type;
        element_type.array_dimensions.clear();
        push_type(element_type, type_ids);
        for (size_t dimension_index = 0; dimension_index < type.array_dimensions.size(); ++dimension_index) {
            begin_instruction(SpvOpTypeArray);
            push_word(array_type_ids[array_type_ids.size() - type.array_dimensions.size() + dimension_index]);
            push_word(dimension_index == 0 ? element_type_id : array_type_ids[array_type_ids.size() - type.array_dimensions.size() + dimension_index - 1]);
            push_word(type.array_dimensions[type.array_dimensions.size() - dimension_index - 1]);
            end_instruction();
        }
    }
}

const std::vector<uint32_t> &SpirvTransform::get_extra_type_ids() const noexcept {
    return *extra_type_ids;
}

void SpirvTransform::begin_instruction(uint16_t opcode) {
    assert_no_instruction();
    instruction_index = transformed->size();
    transformed->push_back(opcode);
}

void SpirvTransform::end_instruction() {
    assert_have_instruction();
    (*transformed)[instruction_index] |= (uint32_t)(transformed->size() - instruction_index) << 16;
    instruction_index = 0;
}

void SpirvTransform::push_word(uint32_t word) {
    assert_have_instruction();
    transformed->push_back(word);
}

void SpirvTransform::push_string_literal(const char *string) {
    // SPIR-V string literals are encoded by splitting a null-terminated UTF-8 string, including the null terminator, into chunks of 4 bytes,
    // padding the string with up to 3 additional null bytes at the end to make its length a multiple of 4 bytes if needed.
    // Then the bytes are encoded as 32-bit words in little-endian byte order:
    // the first byte in each chunk goes in the least-significant byte of the 32-bit word,
    // and the last byte in each chunk goes in the most-significant byte.
    // It turns out that just using memcpy from the string into the words works on both big-endian and little-endian machines.
    assert_have_instruction();
    size_t size = std::strlen(string) + 1;
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word;
        std::strncpy((char *)&word, string + i, 4);
        transformed->push_back(word);
    }
}
