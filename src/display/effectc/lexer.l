/*
 ** effectc/lexer.l
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

%option c++
%option nodefault
%option noyywrap

%option yyclass="effectc_impl::Lexer"
%option prefix="effectc_impl_Lexer_"

%{
    #include <string>

    #include "effectc/impl.h"

    using namespace effectc_impl;

    Lexer::Lexer() : location(nullptr), continuation_length(0), check_for_overlong(false), seen_carriage_return(false) {
    }

    void Lexer::advance_location(char *text, int length) {
        location.begin = location.end;

        // Advance the end of the location by one line for each newline (CR, LF or CRLF) and one column for each UTF-8 character that isn't part of a newline
        for (int i = 0; i < length; ++i) {
            switch (text[i]) {
                case '\r':
                    seen_carriage_return = true;
                    location.end.lines(1);
                    break;
                case '\n':
                    if (!seen_carriage_return) { // Carriage return followed by line feed only counts as one line, not two
                        location.end.lines(1);
                    }
                    seen_carriage_return = false;
                    break;
                default:
                    seen_carriage_return = false;
                    if (continuation_length == 0) {
                        location.end.columns(1);
                        continuation_length = (text[i] & 0xe0) == 0xc0
                            ? 1
                            : (text[i] & 0xf0) == 0xe0
                            ? 2
                            : (text[i] & 0xf8) == 0xf0
                            ? 3
                            : 0;
                        // Disallow UTF-8 overlong encodings
                        if (continuation_length == 1 && text[i] < 0xc2) {
                            continuation_length = 0;
                        } else if ((unsigned char)text[i] == 0xe0 || (unsigned char)text[i] == 0xf0) {
                            check_for_overlong = true;
                        }
                    } else {
                        if (check_for_overlong && ((continuation_length == 2 && (unsigned char)text[i] < 0xa0) || (continuation_length == 3 && (unsigned char)text[i] < 0x90))) {
                            // Disallow UTF-8 overlong encodings
                            continuation_length = 0;
                        } else if ((unsigned char)text[i] < 0x80) {
                            // Disallow non-continuation bytes in the middle of a UTF-8 character
                            continuation_length = 0;
                            location.end.columns(1);
                        } else {
                            --continuation_length;
                        }
                        check_for_overlong = false;
                    }
                    break;
            }
        }
    }

    #define YY_USER_ACTION { \
        advance_location(yytext, yyleng); \
    }

    #define ERROR(msg) do { \
        parser.error(location, msg); \
        return parser.make_YYerror(location); \
    } while (0)
%}

%%

";" {
    return parser.make_semicolon(location);
}

"(" {
    return parser.make_left_parenthesis(location);
}

")" {
    return parser.make_right_parenthesis(location);
}

"[" {
    return parser.make_left_square_bracket(location);
}

"]" {
    return parser.make_right_square_bracket(location);
}

"{" {
    return parser.make_left_curly_brace(location);
}

"}" {
    return parser.make_right_curly_brace(location);
}

"=" {
    return parser.make_operator_assign(location);
}

"?" {
    return parser.make_operator_question_mark(location);
}

":" {
    return parser.make_operator_colon(location);
}

"||" {
    return parser.make_operator_logical_or(location);
}

"&&" {
    return parser.make_operator_logical_and(location);
}

"|" {
    return parser.make_operator_inclusive_or(location);
}

"^" {
    return parser.make_operator_exclusive_or(location);
}

"&" {
    return parser.make_operator_and(location);
}

"==" {
    return parser.make_operator_equal_to(location);
}

"!=" {
    return parser.make_operator_not_equal_to(location);
}

"<" {
    return parser.make_operator_less_than(location);
}

">" {
    return parser.make_operator_greater_than(location);
}

"<=" {
    return parser.make_operator_less_than_or_equal_to(location);
}

">=" {
    return parser.make_operator_greater_than_or_equal_to(location);
}

"<<" {
    return parser.make_operator_shift_left(location);
}

">>" {
    return parser.make_operator_shift_right(location);
}

"+" {
    return parser.make_operator_add(location);
}

"-" {
    return parser.make_operator_subtract(location);
}

"*" {
    return parser.make_operator_multiply(location);
}

"/" {
    return parser.make_operator_divide(location);
}

"%" {
    return parser.make_operator_modulo(location);
}

"!" {
    return parser.make_operator_logical_not(location);
}

"~" {
    return parser.make_operator_not(location);
}

false {
    return parser.make_keyword_false(location);
}

NULL {
    return parser.make_keyword_null(location);
}

pass {
    return parser.make_keyword_pass(location);
}

true {
    return parser.make_keyword_true(location);
}

[ \f\n\r\t\v]+ { }

\n|[^\n] {
    ERROR(std::string("illegal character ") + std::string(yytext, yyleng));
}

<<EOF>> {
    return parser.make_END(location);
}

%%
