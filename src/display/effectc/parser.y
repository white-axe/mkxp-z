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

%token keyword_false "false"
%token keyword_null "NULL"
%token keyword_pass "pass"
%token keyword_true "true"

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
    : type identifier operator_assign initializer semicolon
    ;

initializer
    : assignment_expression
    | list_initializer
    ;

list_initializer
    : left_curly_brace optional_initializer_list right_curly_brace
    ;

optional_initializer_list
    : %empty
    | initializer_list trailing_comma
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
    | string_literals
    | keyword_true
    | keyword_false
    | keyword_null
    ;

string_literals
    : string_literal
    | string_literals string_literal

%%

void effectc_impl::Parser::error(const location &location, const std::string &message) {
    // TODO: the error message should go somewhere other than standard error
    std::cerr << "Error (" << location << "): " << message << std::endl;
}
