/*
 ** spirv/remap-textures.cpp
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
#include "spirv/remap-textures.h"

#include <spirv/unified1/spirv.h>

bool mkxp_spirv_utils::remap_textures(
    const uint32_t *spirv_input,
    size_t spirv_input_size,
    const std::vector<std::pair<std::string, SpirvTexture::Type>> &textures,
    const std::unordered_map<std::string, uint32_t> &texture_id_map,
    std::vector<uint32_t> &spirv_output,
    std::string &error_message
) {
    std::unordered_map<uint32_t, std::string> texture_id_map_reverse;
    for (const auto &texture : texture_id_map) {
        texture_id_map_reverse.emplace(texture.second, texture.first);
    }

    class RemapTexturesTransform : public SpirvTransform
    {
    private:
        const std::vector<std::pair<std::string, SpirvTexture::Type>> &textures;
        const std::unordered_map<std::string, uint32_t> &texture_id_map;
        const std::unordered_map<uint32_t, std::string> &texture_id_map_reverse;

        // SPIR-V IDs for our remapped textures.
        std::vector<uint32_t> remapped_texture_ids;
        // SPIR-V ID of the void type.
        uint32_t void_type_id;
        // A map from SPIR-V image types of void sampled type to their SPIR-V IDs.
        // The format of the key is the 7 operands of OpTypeImage other than the sample type (which is the void type).
        std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>, uint32_t> void_image_type_ids;
        // A map from SPIR-V IDs of images to the SPIR-V IDs of their corresponding sampled image types.
        std::unordered_map<uint32_t, uint32_t> sampled_image_type_ids;
        // A map from SPIR-V IDs of pointers of uniform constant storage class to the SPIR-V IDs of their pointed-to types.
        std::unordered_map<uint32_t, uint32_t> uniform_constant_pointer_type_ids;
        // A map from SPIR-V IDs of variables of uniform constant storage class to their pointer type IDs.
        std::unordered_map<uint32_t, uint32_t> uniform_constant_pointer_ids;

    public:
        RemapTexturesTransform(const std::vector<std::pair<std::string, SpirvTexture::Type>> &textures, const std::unordered_map<std::string, uint32_t> &texture_id_map, const std::unordered_map<uint32_t, std::string> &texture_id_map_reverse) : textures(textures), texture_id_map(texture_id_map), texture_id_map_reverse(texture_id_map_reverse), void_type_id(0) {
        }

    private:
        uint32_t maximum_supported_source_version() const noexcept override {
            return 0x00010600;
        }

        bool begin(std::string &error_message) override {
            remapped_texture_ids.reserve(textures.size());
            for (const std::pair<std::string, SpirvTexture::Type> &texture : textures) {
                uint32_t id;
                auto it = texture_id_map.find(texture.first);
                if (it != texture_id_map.end()) {
                    id = it->second;
                } else {
                    if (!allocate_id(id, error_message)) {
                        return false;
                    }
                }
                remapped_texture_ids.push_back(id);
            }

            return true;
        }

        bool end_name_debug_section(std::string &error_message) override {
            // Declare the names of our remapped textures
            for (uint32_t texture_index = 0; texture_index < textures.size(); ++texture_index) {
                const std::pair<std::string, SpirvTexture::Type> &texture = textures[texture_index];
                uint32_t texture_id = remapped_texture_ids[texture_index];

                begin_instruction(SpvOpName);
                push_word(texture_id);
                push_string_literal(texture.first.c_str());
                end_instruction();
            }

            return true;
        }

        bool end_process_debug_section(std::string &error_message) override {
            begin_instruction(SpvOpModuleProcessed);
            push_string_literal("mkxp_spirv_utils::remap_textures");
            end_instruction();

            return true;
        }

        bool end_annotation_section(std::string &error_message) override {
            // Declare the linkages of our remapped textures
            for (uint32_t texture_index = 0; texture_index < textures.size(); ++texture_index) {
                uint32_t texture_id = remapped_texture_ids[texture_index];

                begin_instruction(SpvOpDecorate);
                push_word(texture_id);
                push_word(SpvDecorationBinding);
                push_word(texture_index);
                end_instruction();

                begin_instruction(SpvOpDecorate);
                push_word(texture_id);
                push_word(SpvDecorationDescriptorSet);
                push_word(0);
                end_instruction();
            }

            return true;
        }

        bool end_data_section(std::string &error_message) override {
            // Declare the void type if it hasn't already been declared
            if (void_type_id == 0) {
                if (!allocate_id(void_type_id, error_message)) {
                    return false;
                }
                begin_instruction(SpvOpTypeVoid);
                push_word(void_type_id);
                end_instruction();
            }

            for (uint32_t texture_index = 0; texture_index < textures.size(); ++texture_index) {
                const std::pair<std::string, SpirvTexture::Type> &texture = textures[texture_index];
                uint32_t texture_id = remapped_texture_ids[texture_index];

                // For any remapped textures whose types have not been declared yet, declare their types
                uint32_t texture_type_id;
                if (texture_id_map_reverse.count(texture_id)) {
                    texture_type_id = uniform_constant_pointer_type_ids.at(uniform_constant_pointer_ids.at(texture_id));
                } else {
                    uint32_t dimensionality;
                    switch (texture.second) {
                        case SpirvTexture::Type::TEXTURE_1D:
                            dimensionality = SpvDim1D;
                            break;
                        case SpirvTexture::Type::UNKNOWN:
                        case SpirvTexture::Type::TEXTURE_2D:
                            dimensionality = SpvDim2D;
                            break;
                        case SpirvTexture::Type::TEXTURE_3D:
                            dimensionality = SpvDim3D;
                            break;
                    }
                    std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t> image_type = std::make_tuple(dimensionality, 0, 0, 0, 1, SpvImageFormatUnknown, SpvAccessQualifierMax);
                    uint32_t image_type_id;
                    {
                        auto it = void_image_type_ids.find(image_type);
                        if (it != void_image_type_ids.end()) {
                            image_type_id = it->second;
                        } else {
                            if (!allocate_id(image_type_id, error_message)) {
                                return false;
                            }
                            begin_instruction(SpvOpTypeImage);
                            push_word(image_type_id);
                            push_word(void_type_id);
                            push_word(std::get<0>(image_type));
                            push_word(std::get<1>(image_type));
                            push_word(std::get<2>(image_type));
                            push_word(std::get<3>(image_type));
                            push_word(std::get<4>(image_type));
                            push_word(std::get<5>(image_type));
                            if (std::get<6>(image_type) != SpvAccessQualifierMax) {
                                push_word(std::get<6>(image_type));
                            }
                            end_instruction();
                            void_image_type_ids.emplace(image_type, image_type_id);
                        }
                    }
                    {
                        auto it = sampled_image_type_ids.find(image_type_id);
                        if (it != sampled_image_type_ids.end()) {
                            texture_type_id = it->second;
                        } else {
                            if (!allocate_id(texture_type_id, error_message)) {
                                return false;
                            }
                            begin_instruction(SpvOpTypeSampledImage);
                            push_word(texture_type_id);
                            push_word(image_type_id);
                            end_instruction();
                            sampled_image_type_ids.emplace(image_type_id, texture_type_id);
                        }
                    }
                }

                // For any remapped textures whose pointer types have not been declared yet, declare their pointer types
                uint32_t texture_pointer_type_id;
                auto texture_pointer_type_it = uniform_constant_pointer_type_ids.find(texture_type_id);
                if (texture_pointer_type_it != uniform_constant_pointer_type_ids.end()) {
                    texture_pointer_type_id = texture_pointer_type_it->second;
                } else {
                    if (!allocate_id(texture_pointer_type_id, error_message)) {
                        return false;
                    }
                    begin_instruction(SpvOpTypePointer);
                    push_word(texture_pointer_type_id);
                    push_word(SpvStorageClassUniformConstant);
                    push_word(texture_type_id);
                    end_instruction();
                    uniform_constant_pointer_type_ids.emplace(texture_type_id, texture_pointer_type_id);
                }

                // Declare our remapped textures
                begin_instruction(SpvOpVariable);
                push_word(texture_pointer_type_id);
                push_word(texture_id);
                push_word(SpvStorageClassUniformConstant);
                end_instruction();
            }

            return true;
        }

        bool receive_instruction(uint16_t opcode, const uint32_t *words, uint16_t num_words, std::string &error_message) override {
            switch (opcode) {
                case SpvOpTypeVoid:
                    void_type_id = words[0];
                    break;

                case SpvOpTypeImage:
                    if (words[1] == void_type_id) {
                        void_image_type_ids.emplace(std::make_tuple(words[2], words[3], words[4], words[5], words[6], words[7], num_words > 8 ? words[8] : SpvAccessQualifierMax), words[0]);
                    }
                    break;

                case SpvOpTypeSampledImage:
                    sampled_image_type_ids.emplace(words[1], words[0]);
                    break;

                case SpvOpTypePointer:
                    if (words[1] == SpvStorageClassUniformConstant) {
                        uniform_constant_pointer_type_ids.emplace(words[0], words[2]);
                    }
                    break;

                case SpvOpVariable:
                    if (words[2] == SpvStorageClassUniformConstant) {
                        uniform_constant_pointer_ids.emplace(words[1], words[0]);
                    }
                    break;

                default:
                    break;
            }

            switch (opcode) {
                case SpvOpEntryPoint:
                    {
                        // Remove the old textures from the OpEntryPoint instruction and add our remapped textures to it
                        begin_instruction(opcode);
                        uint16_t word_index = 2;
                        while ((words[word_index] & 0xff000000) != 0) {
                            ++word_index;
                        }
                        ++word_index;
                        push_words(words, words + word_index);
                        for (; word_index < num_words; ++word_index) {
                            uint32_t id = words[word_index];
                            if (!texture_id_map_reverse.count(id)) {
                                push_word(id);
                            }
                        }
                        push_words(remapped_texture_ids.begin(), remapped_texture_ids.end());
                        end_instruction();
                    }
                    break;

                case SpvOpVariable:
                    // Remove the declarations of the old textures
                    if (!texture_id_map_reverse.count(words[1])) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpName:
                case SpvOpDecorate:
                case SpvOpDecorateId:
                case SpvOpDecorateString:
                    // Remove debug information and annotations from the old textures
                    if (!texture_id_map_reverse.count(words[0])) {
                        begin_instruction(opcode);
                        push_words(words, words + num_words);
                        end_instruction();
                    }
                    break;

                case SpvOpGroupDecorate:
                    // Remove annotations from the old textures
                    begin_instruction(opcode);
                    push_word(words[0]);
                    for (uint16_t word_index = 1; word_index < num_words; ++word_index) {
                        if (!texture_id_map_reverse.count(words[word_index])) {
                            push_word(words[word_index]);
                        }
                    }
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

    return RemapTexturesTransform {
        textures,
        texture_id_map,
        texture_id_map_reverse,
    }.transform(
        spirv_input,
        spirv_input_size,
        spirv_output,
        error_message
    );
}
