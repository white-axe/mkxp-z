/*
** spirv-transform.h
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

#ifndef SPIRV_TRANSFORM_H
#define SPIRV_TRANSFORM_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class SpirvTransform
{
private:
    std::vector<uint32_t> *transformed;
    uint32_t _source_version;
    uint32_t _target_version;
    uint32_t id_bound;
    size_t instruction_index;

public:
    /*
     * Performs the transform on the input SPIR-V module.
     *
     * input: The input SPIR-V module.
     * input_size: The size of the input SPIR-V module, in 32-bit words.
     * output: If the transform succeeds, this will be set to the compiled shader program. Otherwise, this will be unchanged.
     * error_message: If compilation succeeds, this will be unchanged. Otherwise, this will be set to a string containing the error that occurred.
     *
     * Return value: `true` if the transform succeeded, otherwise `false`.
     */
    bool transform(
        const uint32_t *input,
        size_t input_size,
        std::vector<uint32_t> &output,
        std::string &error_message
    );

private:
    /*
     * Returns the maximum SPIR-V version supported by this transform,
     * encoded as a 32-bit word as it appears in the header of a SPIR-V module.
     */
    virtual uint32_t maximum_supported_source_version() const noexcept;

    /*
     * Returns the target SPIR-V version that this transform outputs,
     * encoded as a 32-bit word as it appears in the header of a SPIR-V module.
     */
    virtual uint32_t override_target_version() const noexcept;

    /*
     * This gets run when the transform starts.
     */
    virtual bool begin(std::string &error_message);

    /*
     * This gets run at the beginning of the part of the SPIR-V module's debug section that contains information about the source code of the shader.
     */
    virtual bool begin_source_debug_section(std::string &error_message);

    /*
     * This gets run at the end of the part of the SPIR-V module's debug section that contains information about the source code of the shader.
     */
    virtual bool end_source_debug_section(std::string &error_message);

    /*
     * This gets run at the beginning of the part of the SPIR-V module's debug section that contains information about the names of variables.
     */
    virtual bool begin_name_debug_section(std::string &error_message);

    /*
     * This gets run at the end of the part of the SPIR-V module's debug section that contains information about the names of variables.
     */
    virtual bool end_name_debug_section(std::string &error_message);

    /*
     * This gets run at the beginning of the part of the SPIR-V module's debug section that contains information about the processors that have been applied to the module.
     */
    virtual bool begin_process_debug_section(std::string &error_message);

    /*
     * This gets run at the end of the part of the SPIR-V module's debug section that contains information about the processors that have been applied to the module.
     */
    virtual bool end_process_debug_section(std::string &error_message);

    /*
     * This gets run at the beginning of the SPIR-V module's annotation section.
     */
    virtual bool begin_annotation_section(std::string &error_message);

    /*
     * This gets run at the end of the SPIR-V module's annotation section.
     */
    virtual bool end_annotation_section(std::string &error_message);

    /*
     * This gets run at the beginning of the SPIR-V module's types, constants and variables section.
     */
    virtual bool begin_data_section(std::string &error_message);

    /*
     * This gets run at the end of the SPIR-V module's types, constants and variables section.
     */
    virtual bool end_data_section(std::string &error_message);

    /*
     * This gets run at the beginning of the SPIR-V module's function declaration or function definition section.
     */
    virtual bool begin_code_section(std::string &error_message);

    /*
     * This gets run whenever an instruction is received.
     */
    virtual bool receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message);

protected:
    uint32_t source_version() const noexcept;
    uint32_t target_version() const noexcept;
    bool allocate_id(uint32_t &id_output, std::string &error_message);

private:
    void assert_have_instruction() const noexcept;
    void assert_no_instruction() const noexcept;

protected:
    void begin_instruction(uint16_t opcode);
    void end_instruction();
    void push_word(uint32_t word);
    template <class Iterator> void push_words(Iterator first, Iterator last) {
        assert_have_instruction();
        transformed->insert(transformed->end(), first, last);
    }
    void push_string_literal(const char *string);
};

#endif // SPIRV_TRANSFORM_H
