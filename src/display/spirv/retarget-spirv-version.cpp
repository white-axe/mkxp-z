/*
 ** spirv/retarget-spirv-version.cpp
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

#include <iomanip>
#include <sstream>
#include <unordered_set>

#include "spirv-transform.h"
#include "spirv/retarget-spirv-version.h"

#include <spirv/unified1/spirv.h>

bool mkxp_spirv_utils::retarget_spirv_version(
    const uint32_t *spirv_input,
    size_t spirv_input_size,
    uint32_t target_version,
    bool allow_version_upgrade,
    bool allow_version_downgrade,
    std::vector<uint32_t> &spirv_output,
    std::string &error_message
) {
    class GetVariablesAndTypesTransform : public SpirvTransform
    {
    private:
        // SPIR-V IDs of global variables whose storage classes are not Input or Output.
        std::vector<uint32_t> &non_io_global_variable_ids;
        // SPIR-V IDs of global variables whose storage classes are Input or Output.
        std::unordered_set<uint32_t> &io_variable_ids;
        // SPIR-V IDs of pointer types of StorageBuffer storage class.
        std::unordered_set<uint32_t> &storage_buffer_pointer_type_ids;
        // SPIR-V IDs of types that are pointed to by at least one pointer of StorageBuffer storage class.
        std::unordered_set<uint32_t> &storage_buffer_type_ids;
        // SPIR-V IDs that are struct types.
        std::unordered_set<uint32_t> &struct_type_ids;
    public:
        GetVariablesAndTypesTransform(
            std::vector<uint32_t> &non_io_global_variable_ids,
            std::unordered_set<uint32_t> &io_variable_ids,
            std::unordered_set<uint32_t> &storage_buffer_pointer_type_ids,
            std::unordered_set<uint32_t> &storage_buffer_type_ids,
            std::unordered_set<uint32_t> &struct_type_ids
        ) :
            non_io_global_variable_ids(non_io_global_variable_ids),
            io_variable_ids(io_variable_ids),
            storage_buffer_pointer_type_ids(storage_buffer_pointer_type_ids),
            storage_buffer_type_ids(storage_buffer_type_ids),
            struct_type_ids(struct_type_ids)
        {
        }

    private:
        uint32_t maximum_supported_source_version() const noexcept override {
            return 0x00010600;
        }

        bool receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message) override {
            switch (opcode) {
                case SpvOpVariable:
                    if (words[2] == SpvStorageClassInput || words[2] == SpvStorageClassOutput) {
                        io_variable_ids.insert(words[1]);
                    } else if (words[2] != SpvStorageClassFunction) {
                        non_io_global_variable_ids.push_back(words[1]);
                    }
                    break;

                case SpvOpTypePointer:
                    if (words[1] == SpvStorageClassStorageBuffer) {
                        storage_buffer_pointer_type_ids.insert(words[0]);
                    }
                    break;

                case SpvOpTypeStruct:
                    if (words[1] == SpvStorageClassStorageBuffer) {
                        storage_buffer_type_ids.insert(words[0]);
                    }
                    struct_type_ids.insert(words[0]);
                    break;

                default:
                    break;
            }

            return true;
        }
    };

    std::vector<uint32_t> non_io_global_variable_ids;
    std::unordered_set<uint32_t> io_variable_ids;
    std::unordered_set<uint32_t> storage_buffer_pointer_type_ids;
    std::unordered_set<uint32_t> storage_buffer_type_ids;
    std::unordered_set<uint32_t> struct_type_ids;
    {
        std::vector<uint32_t> dummy;
        if (!GetVariablesAndTypesTransform {
            non_io_global_variable_ids,
            io_variable_ids,
            storage_buffer_pointer_type_ids,
            storage_buffer_type_ids,
            struct_type_ids,
        }.transform(
            spirv_input,
            spirv_input_size,
            dummy,
            error_message
        )) {
            return false;
        }
    }

    class RetargetSpirvVersionTransform : public SpirvTransform
    {
    private:
        uint32_t _target_version;
        bool allow_version_upgrade;
        bool allow_version_downgrade;
        const std::vector<uint32_t> &non_io_global_variable_ids;
        const std::unordered_set<uint32_t> &io_variable_ids;
        const std::unordered_set<uint32_t> &storage_buffer_pointer_type_ids;
        const std::unordered_set<uint32_t> &storage_buffer_type_ids;
        const std::unordered_set<uint32_t> &struct_type_ids;

        // SPIR-V IDs with a Block decoration.
        std::unordered_set<uint32_t> block_ids;
        // SPIR-V IDs with a BufferBlock decoration.
        std::unordered_set<uint32_t> buffer_block_ids;

    public:
        RetargetSpirvVersionTransform(
            uint32_t target_version,
            bool allow_version_upgrade,
            bool allow_version_downgrade,
            const std::vector<uint32_t> &non_io_global_variable_ids,
            const std::unordered_set<uint32_t> &io_variable_ids,
            const std::unordered_set<uint32_t> &storage_buffer_pointer_type_ids,
            const std::unordered_set<uint32_t> &storage_buffer_type_ids,
            const std::unordered_set<uint32_t> &struct_type_ids
        ) :
            _target_version(target_version),
            allow_version_upgrade(allow_version_upgrade),
            allow_version_downgrade(allow_version_downgrade),
            non_io_global_variable_ids(non_io_global_variable_ids),
            io_variable_ids(io_variable_ids),
            storage_buffer_pointer_type_ids(storage_buffer_pointer_type_ids),
            storage_buffer_type_ids(storage_buffer_type_ids),
            struct_type_ids(struct_type_ids)
        {
        }

    private:
        uint32_t maximum_supported_source_version() const noexcept override {
            return 0x00010600;
        }

        uint32_t override_target_version() const noexcept override {
            uint32_t _source_version = source_version();
            return (allow_version_upgrade && _target_version > _source_version) || (allow_version_downgrade && _target_version < _source_version) ? _target_version : _source_version;
        }

        bool end_process_debug_section(std::string &error_message) override {
            begin_instruction(SpvOpModuleProcessed);
            std::stringstream str;
            str << "mkxp_spirv_utils::retarget_spirv_version (source version: 0x" << std::hex << std::setw(8) << std::setfill('0') << source_version() << "; target version: 0x" << std::hex << std::setw(8) << std::setfill('0') << target_version() << ")";
            push_string_literal(str.str().c_str());
            end_instruction();

            return true;
        }

        bool end_annotation_section(std::string &error_message) override {
            if (source_version() <= 0x00010300 && target_version() > 0x00010300) {
                // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                // so redeclare all BufferBlock decorations on struct types as Block decorations instead when transforming from SPIR-V <= 1.3 to SPIR-V > 1.3.
                for (uint32_t id : buffer_block_ids) {
                    if (!struct_type_ids.count(id)) { // We have to check if the old decoration was declared on a struct type since it may instead have been declared on a decoration group created via OpDecorationGroup
                        continue;
                    }
                    begin_instruction(SpvOpDecorate);
                    push_word(id);
                    push_word(SpvDecorationBlock);
                    end_instruction();
                }
            } else if (source_version() > 0x00010300 && target_version() <= 0x00010300) {
                // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                // so redeclare all Block decorations on struct types with at least one pointer of StorageBuffer storage class as BufferBlock decorations instead when transforming from SPIR-V > 1.3 to SPIR-V <= 1.3.
                // There is a possible corner case that can happen here, where a struct type in the source SPIR-V module has a pointer of Uniform storage class and another pointer of StorageBuffer storage class:
                //     OpDecorate %1 Block
                //     %1 = OpTypeStruct
                //     %2 = OpTypePointer Uniform %1
                //     %3 = OpTypePointer StorageBuffer %1
                //     %4 = OpVariable %2 Uniform
                //     %5 = OpVariable %3 StorageBuffer
                // This transform will produce an invalid output SPIR-V module if the input SPIR-V module does this and reads from both variables %4 and %5.
                // However, we can ignore this possibility because generators that generate SPIR-V from a source language will most likely not produce this type of SPIR-V code:
                // if you have a uniform buffer and a storage buffer with the same members, the generator will produce two separate struct types with the same members instead of one struct type.
                for (uint32_t id : block_ids) {
                    if (!struct_type_ids.count(id)) { // We have to check if the old decoration was declared on a struct type since it may instead have been declared on a decoration group created via OpDecorationGroup
                        continue;
                    }
                    begin_instruction(SpvOpDecorate);
                    push_word(id);
                    push_word(storage_buffer_type_ids.count(id) ? SpvDecorationBufferBlock : SpvDecorationBlock);
                    end_instruction();
                }
            }

            return true;
        }

        bool receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message) override {
            switch (opcode) {
                case SpvOpDecorate:
                    if (words[1] == SpvDecorationBlock) {
                        block_ids.insert(words[0]);
                    } else if (words[1] == SpvDecorationBufferBlock) {
                        buffer_block_ids.insert(words[0]);
                    }
                    break;

                case SpvOpGroupDecorate:
                    if (block_ids.count(words[0])) {
                        block_ids.insert(words + 1, words + num_words);
                    } else if (buffer_block_ids.count(words[0])) {
                        buffer_block_ids.insert(words + 1, words + num_words);
                    }
                    break;

                default:
                    break;
            }

            switch (opcode) {
                case SpvOpEntryPoint:
                    if (source_version() >= 0x00010400 && target_version() < 0x00010400) {
                        // Remove variables that are not of input or output storage class from the OpEntryPoint interface if we're transforming from SPIR-V >= 1.4 to SPIR-V < 1.4
                        begin_instruction(opcode);
                        uint16_t word_index = 2;
                        while ((words[word_index] & 0xff000000) != 0) {
                            ++word_index;
                        }
                        ++word_index;
                        push_words(words, words + word_index);
                        for (; word_index < num_words; ++word_index) {
                            uint32_t id = words[word_index];
                            if (io_variable_ids.count(id)) {
                                push_word(id);
                            }
                        }
                        end_instruction();
                    } else if (source_version() < 0x00010400 && target_version() >= 0x00010400) {
                        // Insert every global variable into the OpEntryPoint interface if we're transforming from SPIR-V < 1.4 to SPIR-V >= 1.4
                        begin_instruction(opcode);
                        uint16_t word_index = 2;
                        while ((words[word_index] & 0xff000000) != 0) {
                            ++word_index;
                        }
                        ++word_index;
                        push_words(words, words + word_index);
                        for (std::unordered_set<uint32_t> interface; word_index < num_words; ++word_index) {
                            uint32_t id = words[word_index];
                            if (!interface.count(id)) { // Remove duplicates, as these are not allowed starting from SPIR-V 1.4
                                push_word(id);
                                interface.insert(id);
                            }
                        }
                        push_words(non_io_global_variable_ids.begin(), non_io_global_variable_ids.end());
                        end_instruction();
                    } else {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpModuleProcessed:
                    // This instruction is only supported starting from SPIR-V 1.1
                    if (target_version() >= 0x00010100) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpDecorate:
                    // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                    // so remove all BufferBlock decorations when transforming from SPIR-V <= 1.3 to SPIR-V > 1.3 and
                    // remove all Block decorations when transforming from SPIR-V > 1.3 to SPIR-V <= 1.3,
                    // as we're going to redeclare them later
                    if (!(
                        (source_version() <= 0x00010300 && target_version() > 0x00010300 && words[1] == SpvDecorationBufferBlock)
                            || (source_version() > 0x00010300 && target_version() <= 0x00010300 && words[1] == SpvDecorationBlock)
                    )) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpTypePointer:
                    if (source_version() <= 0x00010300 && target_version() > 0x00010300 && words[1] == SpvStorageClassUniform && storage_buffer_pointer_type_ids.count(words[0])) {
                        // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                        // so change all pointer types of Uniform storage class with a BufferBlock decoration to have StorageBuffer storage class instead when transforming from SPIR-V <= 1.3 to SPIR-V > 1.3
                        begin_instruction(opcode);
                        push_word(words[0]);
                        push_word(SpvStorageClassStorageBuffer);
                        push_word(words[2]);
                        end_instruction();
                    } else if (source_version() > 0x00010300 && target_version() <= 0x00010300 && words[1] == SpvStorageClassStorageBuffer) {
                        // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                        // so change all pointer types of StorageBuffer storage class to have Uniform storage class instead when transforming from SPIR-V > 1.3 to SPIR-V <= 1.3
                        begin_instruction(opcode);
                        push_word(words[0]);
                        push_word(SpvStorageClassUniform);
                        push_word(words[2]);
                        end_instruction();
                    } else {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpVariable:
                    if (source_version() <= 0x00010300 && target_version() > 0x00010300 && words[2] == SpvStorageClassUniform && storage_buffer_pointer_type_ids.count(words[0])) {
                        // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                        // so change all variables of Uniform storage class with a BufferBlock decoration to have StorageBuffer storage class instead when transforming from SPIR-V <= 1.3 to SPIR-V > 1.3
                        begin_instruction(opcode);
                        push_words(words, words + 2);
                        push_word(SpvStorageClassStorageBuffer);
                        push_words(words + 3, words + num_words);
                        end_instruction();
                    } else if (source_version() > 0x00010300 && target_version() <= 0x00010300 && words[2] == SpvStorageClassStorageBuffer) {
                        // BufferBlock decoration with Uniform storage class in SPIR-V <= 1.3 corresponds to Block decoration with StorageBuffer storage class in SPIR-V > 1.3,
                        // so change all variables of StorageBuffer storage class to have Uniform storage class instead when transforming from SPIR-V > 1.3 to SPIR-V <= 1.3
                        begin_instruction(opcode);
                        push_words(words, words + 2);
                        push_word(SpvStorageClassUniform);
                        push_words(words + 3, words + num_words);
                        end_instruction();
                    } else {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpLessOrGreater:
                    // This instruction is replaced with OpFOrdNotEqual after SPIR-V 1.5
                    if (target_version() > 0x00010500) {
                        begin_instruction(SpvOpFOrdNotEqual);
                    } else {
                        begin_instruction(opcode);
                    }
                    push_words(words, words + num_words);
                    end_instruction();
                    break;

                case SpvOpAtomicCompareExchangeWeak:
                    // This instruction is replaced with OpAtomicCompareExchange after SPIR-V 1.3
                    if (target_version() > 0x00010300) {
                        begin_instruction(SpvOpAtomicCompareExchange);
                    } else {
                        begin_instruction(opcode);
                    }
                    push_words(words, words + num_words);
                    end_instruction();
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

    return RetargetSpirvVersionTransform {
        target_version,
        allow_version_upgrade,
        allow_version_downgrade,
        non_io_global_variable_ids,
        io_variable_ids,
        storage_buffer_type_ids,
        storage_buffer_pointer_type_ids,
        struct_type_ids,
    }.transform(
        spirv_input,
        spirv_input_size,
        spirv_output,
        error_message
    );
}
