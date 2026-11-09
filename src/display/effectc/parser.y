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
%parse-param {Lexer &lexer}
%lex-param {Lexer &lexer}

%locations /* Enables Bison's location API, which allows us to attach a location (line number and column number) to error messages */
%define api.location.file none /* Disables generating location.hpp, which Bison does by default if the location API is enabled, since we don't need this file */

%define api.value.type variant /* Allows tokens to hold data inside of them */
%define api.token.constructor /* Generates a `make_` function for each token for constructing it; requires `api.value.type` to be set to `variant` */

%define parse.error detailed /* When the parser encounters an unexpected token, instead of the error message being simply "syntax error", this makes the error message tell you what unexpected token was encountered and give a list of tokens the parser would have accepted in its place */
%define parse.lac full /* Required to avoid possibly incorrect error messages when `parse.error` is set to `detailed` */

%code requires {
    namespace effectc_impl {
        class Lexer;
    }
}

%code top {
    #include <string>

    #include "effectc/impl.h"
}

%token <std::string> identifier "identifier"
%token <std::string> type "type"
%token <uint64_t> integer_literal "integer literal"
%token <double> floating_point_literal "floating-point literal"
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
%token keyword_sampler_comparison_state "SamplerComparisonState"
%token keyword_sampler_state "SamplerState"
%token keyword_shared "shared"
%token keyword_signed "signed"
%token keyword_sizeof "sizeof"
%token keyword_snorm "snorm"
%token keyword_stateblock "stateblock"
%token keyword_stateblock_state "stateblock_state"
%token keyword_static "static"
%token keyword_static_cast "static_cast"
%token keyword_string "string"
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

%start passes

%%

passes
    : %empty
    | passes pass
    ;

pass
    : keyword_pass optional_identifier annotations left_curly_brace state_assignments right_curly_brace
    ;

optional_identifier
    : %empty
    | identifier
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
    : type identifier annotation_subscript operator_assign initializer semicolon
    ;

annotation_subscript
    : %empty
    | left_square_bracket conditional_expression right_square_bracket
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
    : %empty
    | state_assignments state_assignment
    ;

state_assignment
    : identifier state_assignment_subscript operator_assign state_assignment_initializer semicolon
    ;

state_assignment_initializer
    : expression
    | list_initializer
    ;

state_assignment_subscript
    : %empty
    | left_square_bracket integer_literal right_square_bracket
    ;

expression
    : assignment_expression
    | expression operator_comma assignment_expression
    ;

assignment_expression
    : conditional_expression
    ;

conditional_expression /* TODO: it seems the conditional operator specifically is forbidden in state assignments but allowed in annotations, but all other operators are allowed in both state assignments and annotations */
    : logical_or_expression
    | logical_or_expression operator_question_mark expression operator_colon conditional_expression
    ;

logical_or_expression
    : logical_and_expression
    | logical_or_expression operator_logical_or logical_and_expression
    ;

logical_and_expression
    : inclusive_or_expression
    | logical_and_expression operator_logical_and inclusive_or_expression
    ;

inclusive_or_expression
    : exclusive_or_expression
    | inclusive_or_expression operator_inclusive_or exclusive_or_expression
    ;

exclusive_or_expression
    : and_expression
    | exclusive_or_expression operator_exclusive_or and_expression
    ;

and_expression
    : equality_expression
    | and_expression operator_and equality_expression
    ;

equality_expression
    : relational_expression
    | equality_expression operator_equal_to relational_expression
    | equality_expression operator_not_equal_to relational_expression
    ;

relational_expression
    : shift_expression
    | relational_expression operator_less_than shift_expression
    | relational_expression operator_greater_than shift_expression
    | relational_expression operator_less_than_or_equal_to shift_expression
    | relational_expression operator_greater_than_or_equal_to shift_expression
    ;

shift_expression
    : additive_expression
    | shift_expression operator_shift_left additive_expression
    | shift_expression operator_shift_right additive_expression
    ;

additive_expression
    : multiplicative_expression
    | additive_expression operator_add multiplicative_expression
    | additive_expression operator_subtract multiplicative_expression
    ;

multiplicative_expression
    : cast_expression
    | multiplicative_expression operator_multiply cast_expression
    | multiplicative_expression operator_divide cast_expression
    | multiplicative_expression operator_modulo cast_expression
    ;

cast_expression
    : unary_expression
    | left_parenthesis type right_parenthesis cast_expression
    ;

unary_expression
    : postfix_expression
    | operator_logical_not unary_expression
    | operator_not unary_expression
    | operator_add unary_expression
    | operator_subtract unary_expression
    ;

postfix_expression
    : primary_expression
    | type left_parenthesis expression right_parenthesis
    | postfix_expression operator_structure_reference identifier
    ;

primary_expression /* TODO: complete this list of primary expressions */
    : left_parenthesis expression right_parenthesis
    | integer_literal
    | floating_point_literal
    | string_literals
    | keyword_true
    | keyword_false
    | keyword_null
    ;

string_literals
    : string_literal
    | string_literals string_literal
    ;

%%

void effectc_impl::Parser::error(const location &location, const std::string &message) {
    // TODO: the error message should go somewhere other than standard error
    std::cerr << "Error (" << location << "): " << message << std::endl;
}
