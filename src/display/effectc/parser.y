/*
 ** effectc/parser.y
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

%require "3.6" /* The `YYerror` token was added in Bison 3.6, so we require at least Bison 3.6 */
%language "c++"

%define api.namespace {effectc_impl}
%define api.parser.class {Parser}
%parse-param {HlslToSpirv &hlsl_to_spirv}
%parse-param {const char *hlsl_source_code}
%parse-param {Lexer &lexer}
%lex-param {Lexer &lexer}

%locations /* Enables Bison's location API, which allows us to attach a location (line number and column number) to error messages */
%define api.location.file none /* Disables generating location.hpp, which Bison does by default if the location API is enabled, since we don't need this file */

%define api.value.type variant /* Allows tokens to hold data inside of them */
%define api.token.constructor /* Generates a `make_` function for each token for constructing it; requires `api.value.type` to be set to `variant` */

%define parse.error detailed /* When the parser encounters an unexpected token, instead of the error message being simply "syntax error", this makes the error message tell you what unexpected token was encountered and give a list of tokens the parser would have accepted in its place */
%define parse.lac full /* Required to avoid possibly incorrect error messages when `parse.error` is set to `detailed` */

%{
%}

%code requires {
    #include <cstdint>
    #include <string>
    #include <unordered_map>
    #include <utility>
    #include <vector>

    #include "hlsl-to-spirv.h"

    namespace effectc_impl {
        class Lexer;

        class Value
        {
        public:
            enum Type
            {
                // This value is null.
                NULL_TYPE,
                // This value is a named constant.
                NAMED_CONSTANT,
                // This value is an integer.
                INTEGER,
                // This value is a floating-point value.
                FLOATING_POINT,
                // This value is a string.
                STRING,
                // This value is a shader.
                SHADER,
            };

            struct NamedConstant
            {
                // The name of the constant.
                std::string name;

                NamedConstant(std::string &&name);
            };

            struct Integer
            {
                // The integer value of the expression.
                // This may be either a signed or unsigned integer.
                // If signed, the integer is stored in two's complement representation.
                uint32_t value;
                // If true, the integer is signed. If false, the integer is unsigned.
                bool is_signed;

                Integer();
                Integer(uint32_t value, bool is_signed);
            };

            struct FloatingPoint
            {
                // The floating-point value of the expression.
                double value;

                FloatingPoint();
                FloatingPoint(double value);
            };

            struct String
            {
                // The string value of the expression.
                std::string value;

                String(std::string &&value);
            };

            struct Shader
            {
                // The type of shader that this is.
                enum Type
                {
                    // Compute shader.
                    COMPUTE,
                    // Domain shader.
                    DOMAIN_,
                    // Geometry shader.
                    GEOMETRY,
                    // Hull shader.
                    HULL,
                    // Pixel (fragment) shader.
                    PIXEL,
                    // Vertex shader.
                    VERTEX,
                } type;

                // The name of the shader's main function.
                std::string entry_point;

                Shader(Type type, std::string &&entry_point);
            };

        private:
            Type type;
            union
            {
                Value::NamedConstant named_constant;
                Value::Integer integer;
                Value::FloatingPoint floating_point;
                Value::String string;
                Value::Shader shader;
            };

            void dispose() noexcept;

        public:
            Value();
            Value(Value::NamedConstant &&named_constant);
            Value(Value::Integer &&integer);
            Value(Value::FloatingPoint &&floating_point);
            Value(Value::String &&string);
            Value(Value::Shader &&shader);

            ~Value();

            Value(const Value &other);
            Value(Value &&other) noexcept;
            Value &operator=(const Value &other);
            Value &operator=(Value &&other) noexcept;

            static Value make_boolean(bool value);

            Value::Type get_type() const noexcept;
            Value::NamedConstant &as_named_constant();
            Value::Integer &as_integer();
            Value::FloatingPoint &as_floating_point();
            Value::String &as_string();
            Value::Shader &as_shader();
        };
    }
}

%code top {
    #include <cassert>
    #include <cfenv>
    #include <cmath>
    #include <sstream>

    #include "effectc/impl.h"

    using namespace effectc_impl;

    #define ERROR(location, msg) do { \
        error((location), (msg)); \
        YYERROR; \
    } while (0)

    #define ERROR_NOT_A_CONSTANT_EXPRESSION(symbol) do { \
        /* TODO: non-constant expressions cause a syntax error when used in the subscript of an annotation or in a state assignment, but not inside of the initializer of an annotation */ \
        (symbol) = Value::Integer(0, false); /* This is what FXC uses as the value for non-constant expressions inside of an annotation */ \
        break; \
    } while (0)

    static const char *type_name(const Value &value) {
        switch (value.get_type()) {
            case Value::Type::NULL_TYPE: return "null";
            case Value::Type::NAMED_CONSTANT: return "a named constant";
            case Value::Type::INTEGER: return "an integer";
            case Value::Type::FLOATING_POINT: return "a floating-point value";
            case Value::Type::STRING: return "a string";
            case Value::Type::SHADER: return "a shader";
            default: assert(!"unreachable"); break;
        }
    }

    static bool logical_binary_operator_cast(Value &value1, Value &value2) {
        if ((value1.get_type() == Value::Type::INTEGER || value1.get_type() == Value::Type::FLOATING_POINT) && (value2.get_type() == Value::Type::INTEGER || value2.get_type() == Value::Type::FLOATING_POINT)) {
            if (value1.get_type() == Value::Type::FLOATING_POINT) {
                value1 = Value::make_boolean(value1.as_floating_point().value != 0.);
            }
            if (value2.get_type() == Value::Type::FLOATING_POINT) {
                value2 = Value::make_boolean(value2.as_floating_point().value != 0.);
            }
            return true;
        }
        return false;
    }

    static bool integer_binary_operator_cast(Value &value1, Value &value2) {
        if (value1.get_type() == Value::Type::INTEGER && value2.get_type() == Value::Type::INTEGER) {
            // If one operand is a signed integer and the other is an unsigned integer, convert the signed integer to an unsigned integer
            if (value1.as_integer().is_signed != value2.as_integer().is_signed) {
                value1.as_integer().is_signed = false;
                value2.as_integer().is_signed = false;
            }
            return true;
        }
        return false;
    }

    static bool numeric_binary_operator_cast(Value &value1, Value &value2) {
        if ((value1.get_type() == Value::Type::INTEGER || value1.get_type() == Value::Type::FLOATING_POINT) && (value2.get_type() == Value::Type::INTEGER || value2.get_type() == Value::Type::FLOATING_POINT)) {
            // If one operand is a signed integer and the other is an unsigned integer, convert the signed integer to an unsigned integer
            if (value1.get_type() == Value::Type::INTEGER && value2.get_type() == Value::Type::INTEGER && value1.as_integer().is_signed != value2.as_integer().is_signed) {
                value1.as_integer().is_signed = false;
                value2.as_integer().is_signed = false;
            }
            // If one operand is an integer and the other is a floating-point value, convert the integer to a floating-point value, respecting the signedness of the integer
            if (value1.get_type() == Value::Type::INTEGER && value2.get_type() == Value::Type::FLOATING_POINT) {
                value1 = Value::FloatingPoint(value1.as_integer().is_signed ? (double)(int32_t)value1.as_integer().value : (double)value1.as_integer().value);
            }
            if (value2.get_type() == Value::Type::INTEGER && value1.get_type() == Value::Type::FLOATING_POINT) {
                value2 = Value::FloatingPoint(value2.as_integer().is_signed ? (double)(int32_t)value2.as_integer().value : (double)value2.as_integer().value);
            }
            return true;
        }
        return false;
    }
}

%token <std::string> identifier "identifier"
%token <std::string> type_name "type name"
%token <effectc_impl::Value::Integer> integer_literal "integer literal"
%token <effectc_impl::Value::FloatingPoint> floating_point_literal "floating-point literal"
%token <std::string> string_literal "string literal"

%token semicolon ";"
%token left_parenthesis "("
%token right_parenthesis ")"
%token left_square_bracket "["
%token right_square_bracket "]"
%token left_curly_brace "{"
%token right_curly_brace "}"

%token operator_comma ","
%token operator_assign "="
%token operator_question_mark "?"
%token operator_colon ":"
%token operator_logical_or "||"
%token operator_logical_and "&&"
%token operator_inclusive_or "|"
%token operator_exclusive_or "^"
%token operator_and "&"
%token operator_equal_to "=="
%token operator_not_equal_to "!="
%token operator_less_than "<"
%token operator_greater_than ">"
%token operator_less_than_or_equal_to "<="
%token operator_greater_than_or_equal_to ">="
%token operator_shift_left "<<"
%token operator_shift_right ">>"
%token operator_add "+"
%token operator_subtract "-"
%token operator_multiply "*"
%token operator_divide "/"
%token operator_modulo "%"
%token operator_logical_not "!"
%token operator_not "~"
%token operator_increment "++"
%token operator_decrement "--"
%token operator_structure_reference "."

%token keyword_append_structured_buffer "AppendStructuredBuffer"
%token keyword_asm "asm"
%token keyword_asm_fragment "asm_fragment"
%token keyword_auto "auto"
%token keyword_blend_state "BlendState"
%token keyword_break "break"
%token keyword_buffer "Buffer"
%token keyword_byte_address_buffer "ByteAddressBuffer"
%token keyword_case "case"
%token keyword_catch "catch"
%token keyword_cbuffer "cbuffer"
%token keyword_centroid "centroid"
%token keyword_class "class"
%token keyword_column_major "column_major"
%token keyword_compile "compile"
%token keyword_compile_fragment "compile_fragment"
%token keyword_compile_shader "CompileShader"
%token keyword_compute_shader "ComputeShader"
%token keyword_const "const"
%token keyword_const_cast "const_cast"
%token keyword_consume_structured_buffer "ConsumeStructuredBuffer"
%token keyword_continue "continue"
%token keyword_default "default"
%token keyword_delete "delete"
%token keyword_depth_stencil_state "DepthStencilState"
%token keyword_depth_stencil_view "DepthStencilView"
%token keyword_discard "discard"
%token keyword_do "do"
%token keyword_domain_shader "DomainShader"
%token keyword_dynamic_cast "dynamic_cast"
%token keyword_else "else"
%token keyword_enum "enum"
%token keyword_explicit "explicit"
%token keyword_export "export"
%token keyword_extern "extern"
%token keyword_false "false"
%token keyword_for "for"
%token keyword_friend "friend"
%token keyword_fxgroup "fxgroup"
%token keyword_geometry_shader "GeometryShader"
%token keyword_goto "goto"
%token keyword_groupshared "groupshared"
%token keyword_if "if"
%token keyword_in "in"
%token keyword_inline "inline"
%token keyword_inout "inout"
%token keyword_input_patch "InputPatch"
%token keyword_interface "interface"
%token keyword_line "line"
%token keyword_lineadj "lineadj"
%token keyword_linear "linear"
%token keyword_line_stream "LineStream"
%token keyword_matrix "matrix"
%token keyword_mutable "mutable"
%token keyword_namespace "namespace"
%token keyword_new "new"
%token keyword_nointerpolation "nointerpolation"
%token keyword_noperspective "noperspective"
%token keyword_null "NULL"
%token keyword_operator "operator"
%token keyword_out "out"
%token keyword_output_patch "OutputPatch"
%token keyword_packoffset "packoffset"
%token keyword_pass "pass"
%token keyword_pixelfragment "pixelfragment"
%token keyword_pixel_shader "PixelShader"
%token keyword_point "point"
%token keyword_point_stream "PointStream"
%token keyword_precise "precise"
%token keyword_private "private"
%token keyword_protected "protected"
%token keyword_public "public"
%token keyword_rasterizer_state "RasterizerState"
%token keyword_register "register"
%token keyword_reinterpret_cast "reinterpret_cast"
%token keyword_render_target_view "RenderTargetView"
%token keyword_return "return"
%token keyword_row_major "row_major"
%token keyword_rw_buffer "RWBuffer"
%token keyword_rw_byte_address_buffer "RWByteAddressBuffer"
%token keyword_rw_structured_buffer "RWStructuredBuffer"
%token keyword_rw_texture_1d "RWTexture1D"
%token keyword_rw_texture_1d_array "RWTexture1DArray"
%token keyword_rw_texture_2d "RWTexture2D"
%token keyword_rw_texture_2d_array "RWTexture2DArray"
%token keyword_rw_texture_3d "RWTexture3D"
%token keyword_sample "sample"
%token keyword_sampler "sampler"
%token keyword_sampler_1d "sampler1D"
%token keyword_sampler_2d "sampler2D"
%token keyword_sampler_3d "sampler3D"
%token keyword_sampler_comparison_state "SamplerComparisonState"
%token keyword_sampler_cube "samplerCUBE"
%token keyword_sampler_state "sampler_state"
%token keyword_sampler_state_2 "SamplerState"
%token keyword_shared "shared"
%token keyword_signed "signed"
%token keyword_sizeof "sizeof"
%token keyword_snorm "snorm"
%token keyword_stateblock "stateblock"
%token keyword_stateblock_state "stateblock_state"
%token keyword_static "static"
%token keyword_static_cast "static_cast"
%token keyword_string "string"
%token keyword_string_2 "String"
%token keyword_struct "struct"
%token keyword_structured_buffer "StructuredBuffer"
%token keyword_switch "switch"
%token keyword_tbuffer "tbuffer"
%token keyword_technique "technique"
%token keyword_technique10 "technique10"
%token keyword_technique11 "technique11"
%token keyword_template "template"
%token keyword_texture "texture"
%token keyword_texture_1d "Texture1D"
%token keyword_texture_1d_array "Texture1DArray"
%token keyword_texture_2d "Texture2D"
%token keyword_texture_2d_array "Texture2DArray"
%token keyword_texture_2d_ms "Texture2DMS"
%token keyword_texture_2d_ms_array "Texture2DMSArray"
%token keyword_texture_3d "Texture3D"
%token keyword_texture_cube "TextureCube"
%token keyword_texture_cube_array "TextureCubeArray"
%token keyword_this "this"
%token keyword_throw "throw"
%token keyword_triangle "triangle"
%token keyword_triangleadj "triangleadj"
%token keyword_triangle_stream "TriangleStream"
%token keyword_true "true"
%token keyword_try "try"
%token keyword_typedef "typedef"
%token keyword_typename "typename"
%token keyword_uniform "uniform"
%token keyword_union "union"
%token keyword_unorm "unorm"
%token keyword_unsigned "unsigned"
%token keyword_using "using"
%token keyword_vector "vector"
%token keyword_vertexfragment "vertexfragment"
%token keyword_vertex_shader "VertexShader"
%token keyword_virtual "virtual"
%token keyword_void "void"
%token keyword_volatile "volatile"
%token keyword_while "while"

%token END 0 "end of file"

%type <std::unordered_map<std::string, std::vector<uint8_t>>> passes
%type <std::pair<std::string, std::vector<uint8_t>>> pass

%type <std::string> optional_identifier_or_type_name
%type <std::string> identifier_or_type_name

%type <std::vector<uint8_t>> state_assignments
%type <std::vector<uint8_t>> state_assignment

%type <effectc_impl::Value> state_assignment_initializer

%type <effectc_impl::Value> expression
%type <effectc_impl::Value> assignment_expression
%type <effectc_impl::Value> conditional_expression
%type <effectc_impl::Value> logical_or_expression
%type <effectc_impl::Value> logical_and_expression
%type <effectc_impl::Value> inclusive_or_expression
%type <effectc_impl::Value> exclusive_or_expression
%type <effectc_impl::Value> and_expression
%type <effectc_impl::Value> equality_expression
%type <effectc_impl::Value> relational_expression
%type <effectc_impl::Value> shift_expression
%type <effectc_impl::Value> additive_expression
%type <effectc_impl::Value> multiplicative_expression
%type <effectc_impl::Value> cast_expression
%type <effectc_impl::Value> unary_expression
%type <effectc_impl::Value> postfix_expression
%type <effectc_impl::Value> primary_expression

%type <std::string> string_literals

%start passes

%%

passes /* TODO: if there are no shaders, we need to compile the HLSL source code once to make sure it's syntactically valid */
    : %empty {
        $$ = std::unordered_map<std::string, std::vector<uint8_t>>();
    }
    | passes pass {
        $$ = std::move($1);
        $$.emplace(std::move($2));
    }
    ;

pass
    : keyword_pass optional_identifier_or_type_name annotations left_curly_brace state_assignments right_curly_brace {
        $$ = std::make_pair(std::move($2), std::move($5));
    }
    ;

optional_identifier_or_type_name
    : %empty {
        $$ = std::string();
    }
    | identifier_or_type_name {
        $$ = std::move($1);
    }
    ;

identifier_or_type_name
    : identifier {
        $$ = std::move($1);
    }
    | type_name {
        $$ = std::move($1);
    }
    ;

annotations
    : %empty
    | operator_less_than annotation_list operator_greater_than
    ;

annotation_list
    : %empty
    | annotation_list annotation
    ;

annotation
    : optional_qualifiers optional_modifier type identifier_or_type_name optional_subscript operator_assign initializer semicolon
    ;

optional_subscript
    : %empty
    | subscript
    ;

subscript
    : left_square_bracket conditional_expression right_square_bracket
    ;

initializer
    : assignment_expression
    | list_initializer
    ;

list_initializer
    : left_curly_brace initializer_list trailing_comma right_curly_brace
    ;

initializer_list
    : initializer
    | initializer_list operator_comma initializer
    ;

trailing_comma
    : %empty
    | operator_comma
    ;

state_assignments
    : %empty {
        $$ = std::vector<uint8_t>();
    }
    | state_assignments state_assignment {
        $$ = std::move($2);
    }
    ;

state_assignment
    : identifier_or_type_name state_assignment_subscript operator_assign state_assignment_initializer semicolon {
        if ($4.get_type() == Value::Type::NULL_TYPE) {
            $$ = std::vector<uint8_t>();
            break;
        }
        if ($4.get_type() != Value::Type::SHADER) {
            ERROR(@4, std::string("expected a shader or NULL, got ") + type_name($4));
        }
        std::string errors;
        if ($4.as_shader().type != Value::Shader::Type::VERTEX && $4.as_shader().type != Value::Shader::Type::PIXEL) {
            ERROR(@4, std::string("unsupported shader type, must be a vertex shader or pixel shader"));
        }
        if (
            !hlsl_to_spirv.compile(
                $$,
                errors,
                hlsl_source_code,
                $4.as_shader().entry_point.c_str(),
                $4.as_shader().type == Value::Shader::Type::PIXEL ? HlslToSpirv::Stage::FRAGMENT : HlslToSpirv::Stage::VERTEX,
                HlslToSpirv::Dialect::D3D9
            )
        ) {
            std::string error_message = std::string("failed to compile shader function ") + $4.as_shader().entry_point + "():";
            std::istringstream errors_stream(errors);
            std::string errors_line;
            while (std::getline(errors_stream, errors_line)) {
                if (errors_line.empty()) {
                    continue;
                }
                error_message += "\n    ";
                error_message += errors_line;
            }
            ERROR(@4, error_message);
        }
    }
    ;

state_assignment_subscript
    : %empty
    | left_square_bracket integer_literal right_square_bracket
    ;

state_assignment_initializer
    : expression {
        $$ = std::move($1);
    }
    | list_initializer {
        // TODO
        $$ = Value();
    }
    ;

expression
    : assignment_expression {
        $$ = std::move($1);
    }
    | expression operator_comma assignment_expression {
        $$ = std::move($3);
    }
    ;

assignment_expression
    : conditional_expression {
        $$ = std::move($1);
    }
    ;

conditional_expression
    : logical_or_expression {
        $$ = std::move($1);
    }
    | logical_or_expression operator_question_mark expression operator_colon conditional_expression {
        if ($1.get_type() != Value::Type::INTEGER && $1.get_type() != Value::Type::FLOATING_POINT) {
            ERROR(@2, std::string("the condition for the '?:' operator must be numeric, but got ") + type_name($1));
        }
        if (!numeric_binary_operator_cast($3, $5) && $3.get_type() != $5.get_type()) {
            ERROR(@4, std::string("the two expressions for the '?:' operator must have compatible types, but got ") + type_name($3) + " and " + type_name($5));
        }
        ERROR_NOT_A_CONSTANT_EXPRESSION($$);
    }
    ;

logical_or_expression
    : logical_and_expression {
        $$ = std::move($1);
    }
    | logical_or_expression operator_logical_or logical_and_expression {
        if (!logical_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '||' operator to ") + type_name($1) + " and " + type_name($3));
        }
        $$ = Value::make_boolean($1.as_integer().value || $3.as_integer().value);
    }
    ;

logical_and_expression
    : inclusive_or_expression {
        $$ = std::move($1);
    }
    | logical_and_expression operator_logical_and inclusive_or_expression {
        if (!logical_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '&&' operator to ") + type_name($1) + " and " + type_name($3));
        }
        $$ = Value::make_boolean($1.as_integer().value && $3.as_integer().value);
    }
    ;

inclusive_or_expression
    : exclusive_or_expression {
        $$ = std::move($1);
    }
    | inclusive_or_expression operator_inclusive_or exclusive_or_expression {
        if (!integer_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '|' operator to ") + type_name($1) + " and " + type_name($3));
        }
        $$ = Value::Integer($1.as_integer().value | $3.as_integer().value, $1.as_integer().is_signed);
    }
    ;

exclusive_or_expression
    : and_expression {
        $$ = std::move($1);
    }
    | exclusive_or_expression operator_exclusive_or and_expression {
        if (!integer_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '^' operator to ") + type_name($1) + " and " + type_name($3));
        }
        $$ = Value::Integer($1.as_integer().value ^ $3.as_integer().value, $1.as_integer().is_signed);
    }
    ;

and_expression
    : equality_expression {
        $$ = std::move($1);
    }
    | and_expression operator_and equality_expression {
        if (!integer_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '&' operator to ") + type_name($1) + " and " + type_name($3));
        }
        $$ = Value::Integer($1.as_integer().value & $3.as_integer().value, $1.as_integer().is_signed);
    }
    ;

equality_expression
    : relational_expression {
        $$ = std::move($1);
    }
    | equality_expression operator_equal_to relational_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '==' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean($1.as_integer().value == $3.as_integer().value);
        } else {
            $$ = Value::make_boolean($1.as_floating_point().value == $3.as_floating_point().value);
        }
    }
    | equality_expression operator_not_equal_to relational_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '!=' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean($1.as_integer().value != $3.as_integer().value);
        } else {
            $$ = Value::make_boolean($1.as_floating_point().value != $3.as_floating_point().value);
        }
    }
    ;

relational_expression
    : shift_expression {
        $$ = std::move($1);
    }
    | relational_expression operator_less_than shift_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '<' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean($1.as_integer().is_signed ? (int32_t)$1.as_integer().value < (int32_t)$3.as_integer().value : $1.as_integer().value < $3.as_integer().value);
        } else {
            $$ = Value::make_boolean($1.as_floating_point().value < $3.as_floating_point().value);
        }
    }
    | relational_expression operator_greater_than shift_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '>' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean($1.as_integer().is_signed ? (int32_t)$1.as_integer().value > (int32_t)$3.as_integer().value : $1.as_integer().value > $3.as_integer().value);
        } else {
            $$ = Value::make_boolean($1.as_floating_point().value > $3.as_floating_point().value);
        }
    }
    | relational_expression operator_less_than_or_equal_to shift_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '<=' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean($1.as_integer().is_signed ? (int32_t)$1.as_integer().value <= (int32_t)$3.as_integer().value : $1.as_integer().value <= $3.as_integer().value);
        } else {
            $$ = Value::make_boolean($1.as_floating_point().value <= $3.as_floating_point().value);
        }
    }
    | relational_expression operator_greater_than_or_equal_to shift_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '>=' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean($1.as_integer().is_signed ? (int32_t)$1.as_integer().value >= (int32_t)$3.as_integer().value : $1.as_integer().value >= $3.as_integer().value);
        } else {
            $$ = Value::make_boolean($1.as_floating_point().value >= $3.as_floating_point().value);
        }
    }
    ;

shift_expression
    : additive_expression {
        $$ = std::move($1);
    }
    | shift_expression operator_shift_left additive_expression {
        if ($1.get_type() != Value::Type::INTEGER || $3.get_type() != Value::Type::INTEGER) {
            ERROR(@2, std::string("cannot apply '<<' operator to ") + type_name($1) + " and " + type_name($3));
        }
        // Shifting by at least the width of the first operand is undefined behavior, so we need to check for it explicitly
        if ($3.as_integer().value >= 32) {
            $$ = Value::Integer(0, $1.as_integer().is_signed);
        } else {
            $$ = Value::Integer($1.as_integer().value << $3.as_integer().value, $1.as_integer().is_signed);
        }
    }
    | shift_expression operator_shift_right additive_expression {
        if ($1.get_type() != Value::Type::INTEGER || $3.get_type() != Value::Type::INTEGER) {
            ERROR(@2, std::string("cannot apply '>>' operator to ") + type_name($1) + " and " + type_name($3));
        }
        // Shifting by at least the width of the first operand is undefined behavior, so we need to check for it explicitly
        if ($3.as_integer().value >= 32) {
            $$ = Value::Integer(0, $1.as_integer().is_signed);
        } else {
            $$ = Value::Integer($1.as_integer().value >> $3.as_integer().value, $1.as_integer().is_signed);
        }
    }
    ;

additive_expression
    : multiplicative_expression {
        $$ = std::move($1);
    }
    | additive_expression operator_add multiplicative_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '+' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::Integer($1.as_integer().value + $3.as_integer().value, $1.as_integer().is_signed);
        } else {
            $$ = Value::FloatingPoint($1.as_floating_point().value + $3.as_floating_point().value);
        }
    }
    | additive_expression operator_subtract multiplicative_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '-' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            $$ = Value::Integer($1.as_integer().value - $3.as_integer().value, $1.as_integer().is_signed);
        } else {
            $$ = Value::FloatingPoint($1.as_floating_point().value - $3.as_floating_point().value);
        }
    }
    ;

multiplicative_expression
    : cast_expression {
        $$ = std::move($1);
    }
    | multiplicative_expression operator_multiply cast_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '*' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            // Signed integer overflow is undefined behavior, so expand to 64 bits before multiplying if we're doing signed multiplication
            $$ = Value::Integer($1.as_integer().is_signed ? (uint32_t)((int64_t)(int32_t)$1.as_integer().value * (int64_t)(int32_t)$3.as_integer().value) : $1.as_integer().value * $3.as_integer().value, $1.as_integer().is_signed);
        } else {
            $$ = Value::FloatingPoint($1.as_floating_point().value * $3.as_floating_point().value);
        }
    }
    | multiplicative_expression operator_divide cast_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '/' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            if ($3.as_integer().value == 0) {
                ERROR_NOT_A_CONSTANT_EXPRESSION($$);
            } else {
                $$ = Value::Integer($1.as_integer().is_signed ? (uint32_t)((int32_t)$1.as_integer().value / (int32_t)$3.as_integer().value) : $1.as_integer().value / $3.as_integer().value, $1.as_integer().is_signed);
            }
        } else {
            if ($3.as_floating_point().value == 0.) {
                ERROR_NOT_A_CONSTANT_EXPRESSION($$);
            } else {
                $$ = Value::FloatingPoint($1.as_floating_point().value / $3.as_floating_point().value);
            }
        }
    }
    | multiplicative_expression operator_modulo cast_expression {
        if (!numeric_binary_operator_cast($1, $3)) {
            ERROR(@2, std::string("cannot apply '%' operator to ") + type_name($1) + " and " + type_name($3));
        }
        if ($1.get_type() == Value::Type::INTEGER) {
            if ($3.as_integer().value == 0) {
                ERROR_NOT_A_CONSTANT_EXPRESSION($$);
            } else {
                $$ = Value::Integer($1.as_integer().is_signed ? (uint32_t)((int32_t)$1.as_integer().value % (int32_t)$3.as_integer().value) : $1.as_integer().value % $3.as_integer().value, $1.as_integer().is_signed);
            }
        } else {
            if ($3.as_floating_point().value == 0.) {
                ERROR_NOT_A_CONSTANT_EXPRESSION($$);
            } else if (!std::isfinite($1.as_floating_point().value)) {
                $$ = Value::FloatingPoint((double)NAN);
            } else {
                $$ = Value::FloatingPoint(std::fmod($1.as_integer().value, $3.as_integer().value));
            }
        }
    }
    ;

cast_expression
    : unary_expression {
        $$ = std::move($1);
    }
    | left_parenthesis type optional_subscripts right_parenthesis cast_expression {
        // TODO
        $$ = std::move($5);
    }
    | left_parenthesis modifier type optional_subscripts right_parenthesis cast_expression {
        // TODO
        $$ = std::move($6);
    }
    | left_parenthesis qualifiers optional_modifier type optional_subscripts right_parenthesis cast_expression {
        // TODO
        $$ = std::move($7);
    }
    ;

optional_subscripts
    : %empty
    | optional_subscripts subscript

unary_expression
    : postfix_expression {
        $$ = std::move($1);
    }
    | operator_logical_not cast_expression {
        if ($2.get_type() != Value::Type::INTEGER && $2.get_type() != Value::Type::FLOATING_POINT) {
            ERROR(@1, std::string("cannot apply '!' operator to ") + type_name($2));
        }
        if ($2.get_type() == Value::Type::INTEGER) {
            $$ = Value::make_boolean(!$2.as_integer().value);
        } else {
            $$ = Value::make_boolean(!$2.as_floating_point().value);
        }
    }
    | operator_not cast_expression {
        if ($2.get_type() != Value::Type::INTEGER) {
            ERROR(@1, std::string("cannot apply '~' operator to ") + type_name($2));
        }
        $$ = Value::Integer(~$2.as_integer().value, $2.as_integer().is_signed);
    }
    | operator_add cast_expression {
        if ($2.get_type() != Value::Type::INTEGER && $2.get_type() != Value::Type::FLOATING_POINT) {
            ERROR(@1, std::string("cannot apply '+' operator to ") + type_name($2));
        }
        $$ = std::move($2);
    }
    | operator_subtract cast_expression {
        if ($2.get_type() != Value::Type::INTEGER && $2.get_type() != Value::Type::FLOATING_POINT) {
            ERROR(@1, std::string("cannot apply '-' operator to ") + type_name($2));
        }
        if ($2.get_type() == Value::Type::INTEGER) {
            $$ = Value::Integer(-$2.as_integer().value, $2.as_integer().is_signed);
        } else {
            $$ = Value::FloatingPoint(-$2.as_floating_point().value);
        }
    }
    ;

postfix_expression
    : primary_expression {
        $$ = std::move($1);
    }
    | type left_parenthesis arguments right_parenthesis {
        // TODO
        $$ = Value();
    }
    | postfix_expression subscript {
        // TODO
        $$ = std::move($1);
    }
    | postfix_expression operator_structure_reference identifier_or_type_name {
        // TODO
        $$ = std::move($1);
    }
    ;

arguments
    : %empty
    | argument_list
    ;

argument_list
    : assignment_expression
    | argument_list operator_comma assignment_expression
    ;

primary_expression
    : left_parenthesis expression right_parenthesis {
        $$ = std::move($2);
    }
    | identifier {
        $$ = Value::NamedConstant(std::move($1));
    }
    | integer_literal {
        $$ = std::move($1);
    }
    | floating_point_literal {
        $$ = std::move($1);
    }
    | string_literals {
        $$ = Value::String(std::move($1));
    }
    | keyword_true {
        $$ = Value::make_boolean(true);
    }
    | keyword_false {
        $$ = Value::make_boolean(false);
    }
    | keyword_null {
        $$ = Value();
    }
    | keyword_compile identifier identifier left_parenthesis arguments right_parenthesis {
        static const std::unordered_map<std::string, Value::Shader::Type> table = {
            {"cs_4_0", Value::Shader::COMPUTE},
            {"cs_4_1", Value::Shader::COMPUTE},
            {"cs_5_0", Value::Shader::COMPUTE},
            {"ds_5_0", Value::Shader::DOMAIN_},
            {"gs_4_0", Value::Shader::GEOMETRY},
            {"gs_4_1", Value::Shader::GEOMETRY},
            {"gs_5_0", Value::Shader::GEOMETRY},
            {"hs_5_0", Value::Shader::HULL},
            {"ps_1_0", Value::Shader::PIXEL},
            {"ps_1_1", Value::Shader::PIXEL},
            {"ps_1_2", Value::Shader::PIXEL},
            {"ps_1_3", Value::Shader::PIXEL},
            {"ps_1_4", Value::Shader::PIXEL},
            {"ps_2_0", Value::Shader::PIXEL},
            {"ps_2_a", Value::Shader::PIXEL},
            {"ps_2_b", Value::Shader::PIXEL},
            {"ps_2_sw", Value::Shader::PIXEL},
            {"ps_3_0", Value::Shader::PIXEL},
            {"ps_3_sw", Value::Shader::PIXEL},
            {"ps_4_0", Value::Shader::PIXEL},
            {"ps_4_0_level_9_1", Value::Shader::PIXEL},
            {"ps_4_0_level_9_3", Value::Shader::PIXEL},
            {"ps_4_1", Value::Shader::PIXEL},
            {"ps_5_0", Value::Shader::PIXEL},
            {"vs_1_0", Value::Shader::VERTEX},
            {"vs_1_1", Value::Shader::VERTEX},
            {"vs_2_0", Value::Shader::VERTEX},
            {"vs_2_a", Value::Shader::VERTEX},
            {"vs_2_sw", Value::Shader::VERTEX},
            {"vs_3_0", Value::Shader::VERTEX},
            {"vs_3_sw", Value::Shader::VERTEX},
            {"vs_4_0", Value::Shader::VERTEX},
            {"vs_4_0_level_9_1", Value::Shader::VERTEX},
            {"vs_4_0_level_9_3", Value::Shader::VERTEX},
            {"vs_4_1", Value::Shader::VERTEX},
            {"vs_5_0", Value::Shader::VERTEX},
        };
        auto it = table.find($2);
        if (it == table.end()) {
            ERROR(@2, "unsupported compiler target " + $2);
        }
        $$ = Value::Shader(it->second, std::move($3));
    }
    ;

string_literals
    : string_literal {
        $$ = std::move($1);
    }
    | string_literals string_literal {
        $1 += $2;
        $$ = std::move($1);
    }
    ;

optional_qualifiers
    : %empty
    | qualifiers qualifier
    ;

qualifiers
    : qualifier
    | qualifiers qualifier
    ;

qualifier
    : keyword_const
    | keyword_column_major
    | keyword_row_major
    ;

optional_modifier
    : %empty
    | modifier
    ;

modifier
    : keyword_unsigned
    | keyword_snorm
    | keyword_unorm
    ;

type
    : type_name
    | string_type
    | keyword_vector vector_template
    | keyword_matrix matrix_template
    ;

string_type
    : keyword_string
    | keyword_string_2
    ;

vector_template
    : %empty
    | operator_less_than optional_modifier type operator_comma additive_expression operator_greater_than
    ;

matrix_template
    : %empty
    | operator_less_than optional_modifier type operator_comma additive_expression operator_comma additive_expression operator_greater_than
    ;

%%

void Parser::error(const location &location, const std::string &message) {
    // TODO: the error message should go somewhere other than standard error
    std::cerr << "Error (" << location << "): " << message << std::endl;
}

Value::NamedConstant::NamedConstant(std::string &&name) : name(std::move(name)) {
}

Value::Integer::Integer() : Integer(0, true) {
}

Value::Integer::Integer(uint32_t value, bool is_signed) : value(value), is_signed(is_signed) {
}

Value::FloatingPoint::FloatingPoint() : FloatingPoint(0.0) {
}

Value::FloatingPoint::FloatingPoint(double value) : value(value) {
}

Value::String::String(std::string &&value) : value(std::move(value)) {
}

Value::Shader::Shader(Value::Shader::Type type, std::string &&entry_point) : type(type), entry_point(std::move(entry_point)) {
}

Value::Value() : type(Type::NULL_TYPE) {
}

Value::Value(Value::NamedConstant &&named_constant) : type(Type::NAMED_CONSTANT), named_constant(std::move(named_constant)) {
}

Value::Value(Value::Integer &&integer) : type(Type::INTEGER), integer(std::move(integer)) {
}

Value::Value(Value::FloatingPoint &&floating_point) : type(Type::FLOATING_POINT), floating_point(std::move(floating_point)) {
}

Value::Value(Value::String &&string) : type(Type::STRING), string(std::move(string)) {
}

Value::Value(Value::Shader &&shader) : type(Type::SHADER), shader(std::move(shader)) {
}

void Value::dispose() noexcept {
    switch (type) {
        case Type::NULL_TYPE: break;
        case Type::NAMED_CONSTANT: named_constant.~NamedConstant(); break;
        case Type::INTEGER: integer.~Integer(); break;
        case Type::FLOATING_POINT: floating_point.~FloatingPoint(); break;
        case Type::STRING: string.~String(); break;
        case Type::SHADER: shader.~Shader(); break;
        default: assert(!"unreachable"); break;
    }
}

Value::~Value() {
     dispose();
}

Value::Value(const Value &other) {
    *this = other;
}

Value::Value(Value &&other) noexcept {
    *this = std::move(other);
}

Value &Value::operator=(const Value &other) {
    dispose();
    switch ((type = other.type)) {
        case Type::NULL_TYPE: break;
        case Type::NAMED_CONSTANT: new(&named_constant) NamedConstant(other.named_constant); break;
        case Type::INTEGER: new(&integer) Integer(other.integer); break;
        case Type::FLOATING_POINT: new(&floating_point) FloatingPoint(other.floating_point); break;
        case Type::STRING: new(&string) String(other.string); break;
        case Type::SHADER: new(&shader) Shader(other.shader); break;
        default: assert(!"unreachable"); break;
    }
    return *this;
}

Value &Value::operator=(Value &&other) noexcept {
    dispose();
    switch ((type = other.type)) {
        case Type::NULL_TYPE: break;
        case Type::NAMED_CONSTANT: new(&named_constant) NamedConstant(std::move(other.named_constant)); break;
        case Type::INTEGER: new(&integer) Integer(std::move(other.integer)); break;
        case Type::FLOATING_POINT: new(&floating_point) FloatingPoint(std::move(other.floating_point)); break;
        case Type::STRING: new(&string) String(std::move(other.string)); break;
        case Type::SHADER: new(&shader) Shader(std::move(other.shader)); break;
        default: assert(!"unreachable"); break;
    }
    return *this;
}

Value Value::make_boolean(bool value) {
    return Value::Integer(value ? 1 : 0, true);
}

Value::Type Value::get_type() const noexcept {
    return type;
}

Value::NamedConstant &Value::as_named_constant() {
    if (type == Type::NAMED_CONSTANT) {
        return named_constant;
    } else {
        throw std::bad_cast();
    }
}

Value::Integer &Value::as_integer() {
    if (type == Type::INTEGER) {
        return integer;
    } else {
        throw std::bad_cast();
    }
}

Value::FloatingPoint &Value::as_floating_point() {
    if (type == Type::FLOATING_POINT) {
        return floating_point;
    } else {
        throw std::bad_cast();
    }
}

Value::String &Value::as_string() {
    if (type == Type::STRING) {
        return string;
    } else {
        throw std::bad_cast();
    }
}

Value::Shader &Value::as_shader() {
    if (type == Type::SHADER) {
        return shader;
    } else {
        throw std::bad_cast();
    }
}
