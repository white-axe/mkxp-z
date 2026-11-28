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
#include <iomanip>
#include <sstream>
#include <utility>

// _byteswap_ulong
#ifdef _MSC_VER
#  include <stdlib.h>
#endif // _MSC_VER

#include "spirv-transform.h"

#include <spirv/unified1/spirv.h>

static uint32_t byteswap_word(uint32_t word) {
#ifdef _MSC_VER
    static_assert(sizeof(unsigned long) == sizeof(uint32_t), "unsigned long should be 32 bits");
    return (uint32_t)_byteswap_ulong((unsigned long)word);
#else
    return __builtin_bswap32(word);
#endif // _MSC_VAR
}

static uint32_t maybe_byteswap_word(uint32_t word, bool needs_byteswap) {
    return needs_byteswap ? byteswap_word(word) : word;
}

bool SpirvTransform::transform(
    const uint32_t *input,
    size_t input_size,
    std::vector<uint32_t> &output,
    std::string &error_message
) {
    std::vector<uint32_t> _transformed;
    transformed = &_transformed;
    instruction_index = 0;

    bool needs_byteswap;

    bool seen_source_debug_section = false;
    bool seen_name_debug_section = false;
    bool seen_process_debug_section = false;
    bool seen_annotation_section = false;
    bool seen_data_section = false;
    bool seen_code_section = false;

    if (input_size < 5) {
        error_message = "Invalid SPIR-V module";
        return false;
    }

    {
        uint32_t magic = input[0];
        if (magic == SpvMagicNumber) {
            needs_byteswap = false;
        } else if (magic == byteswap_word(SpvMagicNumber)) {
            needs_byteswap = true;
        } else {
            error_message = "Invalid SPIR-V module";
            return false;
        }
    }

    _target_version = _source_version = maybe_byteswap_word(input[1], needs_byteswap);

    // The newest SPIR-V version that we support right now is SPIR-V 1.6.
    // Newer SPIR-V versions, if any are released,
    // may have additional instructions that are allowed in each section of the SPIR-V module,
    // which would break our detection for where the beginning of each section is,
    // so we need to explicitly check the version.
    if (_source_version > 0x00010600) {
        std::stringstream str;
        str << "Unsupported SPIR-V version 0x" << std::hex << std::setw(8) << std::setfill('0') << _source_version << "; versions later than SPIR-V 1.6 are currently unsupported";
        error_message = str.str();
        return false;
    }

    id_bound = maybe_byteswap_word(input[3], needs_byteswap);

    {
        uint32_t minimum = minimum_supported_source_version();
        uint32_t maximum = maximum_supported_source_version();
        if (_source_version < minimum || _source_version > maximum) {
            std::stringstream str;
            str << "Unsupported SPIR-V version 0x" << std::hex << std::setw(8) << std::setfill('0') << _source_version << "; must be greater than or equal to 0x" << std::hex << std::setw(8) << std::setfill('0') << minimum << " and less than or equal to 0x" << std::hex << std::setw(8) << std::setfill('0') << maximum;
            error_message = str.str();
            return false;
        }
    }

    _target_version = override_target_version();

    _transformed.push_back(input[0]);
    _transformed.push_back(_target_version);
    _transformed.insert(_transformed.end(), input + 2, input + 5);

    if (!begin(error_message)) {
        return false;
    }
    assert_no_instruction();

    for (size_t word_index = 5; word_index < input_size;) {
        uint16_t opcode;
        uint16_t instruction_size;
        {
            uint32_t word = maybe_byteswap_word(input[word_index], needs_byteswap);
            opcode = word;
            instruction_size = word >> 16;
        }
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

            if (target_version() >= 0x00010100) {
                if (!begin_process_debug_section(error_message)) {
                    return false;
                }
                assert_no_instruction();
            }
        }

        if (is_start_of_annotation_section) {
            if (target_version() >= 0x00010100) {
                if (!end_process_debug_section(error_message)) {
                    return false;
                }
                assert_no_instruction();
            }

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

        {
            const uint32_t *words = input + (word_index + 1);
            uint16_t num_words = instruction_size - 1;
            if (needs_byteswap) {
                std::vector<uint32_t> byteswapped_input;
                byteswapped_input.reserve(num_words);
                for (uint16_t i = 0; i < num_words; ++i) {
                    byteswapped_input.push_back(byteswap_word(words[i]));
                }
                if (!receive_instruction(
                    opcode,
                    byteswapped_input.data(),
                    byteswapped_input.size(),
                    error_message
                )) {
                    return false;
                }
            } else {
                if (!receive_instruction(
                    opcode,
                    words,
                    num_words,
                    error_message
                )) {
                    return false;
                }
            }
        }
        assert_no_instruction();

        word_index += instruction_size;
    }

    _transformed[3] = id_bound;
    output = std::move(_transformed);
    return true;
}

uint32_t SpirvTransform::minimum_supported_source_version() const noexcept {
    return 0x00010000;
}

uint32_t SpirvTransform::maximum_supported_source_version() const noexcept {
    return UINT32_MAX;
}

uint32_t SpirvTransform::override_target_version() const noexcept {
    return source_version();
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

uint32_t SpirvTransform::source_version() const noexcept {
    return _source_version;
}

uint32_t SpirvTransform::target_version() const noexcept {
    return _target_version;
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
