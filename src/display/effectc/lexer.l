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

%option prefix="effectc_impl_Lexer_"

%option nodefault
%option noyywrap
%option reentrant

%{
    #include <climits>
    #include <cstdlib>
    #include <cstring>
    #include <sstream>
    #include <string>
    #include <utility>

    #include "effectc/impl.h"

    using namespace effectc_impl;

    #define YY_USER_ACTION { \
        advance_location(yytext, yyleng); \
    }

    #define ERROR(msg) do { \
        parser.error(location, (msg)); \
        return parser.make_YYerror(location); \
    } while (0)

    static Parser::symbol_type parse_integer(Parser &parser, const char *str, int base, const Parser::location_type &location) {
        static_assert(UINT64_MAX <= ULLONG_MAX, "uint64_t cannot fit into unsigned long long");
        errno = 0;
        unsigned long long value = std::strtoull(str, nullptr, base);
        if (errno != 0) {
            errno = 0;
            ERROR("integer literal out of range");
        }
        if (value > UINT64_MAX) {
            ERROR("integer literal out of range");
        }
        return parser.make_integer_literal((uint64_t)value, location);
    }

    static Parser::symbol_type parse_floating_point(Parser &parser, const char *str, const Parser::location_type &location) {
        errno = 0;
        double value = std::strtod(str, nullptr);
        if (errno != 0) {
            errno = 0;
            ERROR("floating-point literal out of range");
        }
        return parser.make_floating_point_literal(value, location);
    }
%}

%%

\/\*([^*]*|(\*+[^*/]))*\*+\/ { /* Multi-line comment */ }

\/\*([^*]*|(\*+[^*/]))*\** {
    ERROR("unterminated multi-line comment");
}

\/\/[^\n\r]* { /* Single-line comment */ }

\"[^\"\n\r]*\" { // String literal
    std::string string;
    string.reserve(yyleng - 2);
    const char *p = yytext + 1;
    for (bool is_reading_text = true; is_reading_text;) {
        const char c = *p;
        switch (c) {
            case '"':
                is_reading_text = false;
                break;
            case '\n':
                string.push_back(c);
                ++p;
                break;
            default:
                string.push_back(c);
                ++p;
                break;
            case '\\':
                ++p;
                if (*p >= '0' && *p <= '7') { // Octal escape sequence
                    if (p[1] < '0' || p[1] > '7' || p[2] < '0' || p[2] > '7') {
                        ERROR("invalid octal escape sequence");
                    }
                    uint16_t codepoint = *p - '0';
                    codepoint <<= 3;
                    codepoint |= p[1] - '0';
                    codepoint <<= 3;
                    codepoint |= p[2] - '0';
                    if (codepoint <= 0x7f) {
                        string.push_back(codepoint);
                    } else {
                        string.push_back(0xc0 | (codepoint >> 6));
                        string.push_back(0x80 | (codepoint & 0x3f));
                    }
                    p += 3;
                } else {
                    switch (*p) {
                        case 'x': // Hexadecimal escape sequence
                            {
                                if ((p[1] < '0' || p[1] > '9') && (p[1] < 'A' || p[1] > 'F') && (p[1] < 'a' || p[1] > 'f')) {
                                    ERROR("invalid hexadecimal escape sequence");
                                }
                                uint32_t codepoint = 0;
                                for (++p; (*p >= '0' && *p <= '9') || (*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'); ++p) {
                                    codepoint <<= 4;
                                    codepoint |= *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'A' && *p <= 'F' ? *p - 'A' + 0xa : *p - 'a' + 0xa;
                                    // Disallow codepoints that are out of the Unicode range
                                    if (codepoint > 0x10ffff) {
                                        ERROR("invalid hexadecimal escape sequence");
                                    }
                                }
                                // Disallow codepoints that are UTF-16 surrogates
                                if (codepoint >= 0xd800 && codepoint <= 0xdfff) {
                                    ERROR("invalid hexadecimal escape sequence");
                                }
                                // Encode the codepoint as one to four UTF-8 bytes
                                if (codepoint <= 0x7f) {
                                    string.push_back(codepoint);
                                } else if (codepoint <= 0x7ff) {
                                    string.push_back(0xc0 | (codepoint >> 6));
                                    string.push_back(0x80 | (codepoint & 0x3f));
                                } else if (codepoint <= 0xffff) {
                                    string.push_back(0xe0 | (codepoint >> 12));
                                    string.push_back(0x80 | ((codepoint >> 6) & 0x3f));
                                    string.push_back(0x80 | (codepoint & 0x3f));
                                } else {
                                    string.push_back(0xe8 | (codepoint >> 18));
                                    string.push_back(0x80 | ((codepoint >> 12) & 0x3f));
                                    string.push_back(0x80 | ((codepoint >> 6) & 0x3f));
                                    string.push_back(0x80 | (codepoint & 0x3f));
                                }
                            }
                            break;
                        case '\n':
                            ++p;
                            break;
                        case '\r':
                            if (p[1] == '\n') {
                                p += 2;
                            } else {
                                ++p;
                            }
                            break;
                        case 'a':
                            string.push_back('\a');
                            ++p;
                            break;
                        case 'b':
                            string.push_back('\b');
                            ++p;
                            break;
                        case 'f':
                            string.push_back('\f');
                            ++p;
                            break;
                        case 'n':
                            string.push_back('\n');
                            ++p;
                            break;
                        case 'r':
                            string.push_back('\r');
                            ++p;
                            break;
                        case 't':
                            string.push_back('\t');
                            ++p;
                            break;
                        case 'v':
                            string.push_back('\v');
                            ++p;
                            break;
                        default: // Silently ignore the backslash if the first character after the backslash is not a valid starting character for an escape sequence
                            string.push_back(*p);
                            ++p;
                            break;
                    }
                }
                break;
        }
    }
    return parser.make_string_literal(string, location);
}

\"[^\"\n\r]* {
    ERROR("unterminated string literal");
}

0[xX][0-9A-Fa-f]+([uU][lL]?|[lL][uU]?)? { // Hexadecimal integer literal
    char *str = yytext + 2;
    char *ptr = std::strpbrk(str, "uUlL");
    if (ptr != nullptr) {
        *ptr = 0;
    }
    return parse_integer(parser, str, 16, location);
}

0[0-7]*([uU][lL]?|[lL][uU]?)? { // Octal integer literal
    char *str = yytext + 1;
    char *ptr = std::strpbrk(str, "uUlL");
    if (ptr != nullptr) {
        *ptr = 0;
    }
    return parse_integer(parser, str, 8, location);
}

[1-9][0-9]*([uU][lL]?|[lL][uU]?)? { // Decimal integer literal
    char *ptr = std::strpbrk(yytext, "uUlL");
    if (ptr != nullptr) {
        *ptr = 0;
    }
    return parse_integer(parser, yytext, 10, location);
}

(([0-9]*\.[0-9]+|[0-9]+\.)([eE][+-]?[0-9]+)?|[0-9]+[eE][+-]?[0-9]+)[hHfFlL]? { // Floating-point literal
    char *ptr = std::strpbrk(yytext, "hHfFlL");
    if (ptr != nullptr) {
        *ptr = 0;
    }
    return parse_floating_point(parser, yytext, location);
}

[0-9][0-9A-Za-z_.]* {
    ERROR("invalid numeric literal");
}

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

"," {
    return parser.make_operator_comma(location);
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

"++" {
    return parser.make_operator_increment(location);
}

"--" {
    return parser.make_operator_decrement(location);
}

"." {
    return parser.make_operator_structure_reference(location);
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
    std::stringstream error;
    error << "illegal byte 0x" << ((unsigned char)yytext[0] <= 0x0f ? "0" : "") << std::hex << (unsigned int)yytext[0] << " in input";
    ERROR(error.str());
}

<<EOF>> {
    return parser.make_END(location);
}

%%

Lexer::Lexer() : location(nullptr), continuation_length(0), check_for_overlong(false), seen_carriage_return(false) {
    yylex_init(&yyscanner);
}

Lexer::~Lexer() {
    if (yyscanner != nullptr) {
        yylex_destroy(yyscanner);
    }
}

Lexer::Lexer(Lexer &&other) noexcept {
    *this = std::move(other);
}

Lexer &Lexer::operator=(Lexer &&other) noexcept {
    yyscanner = other.yyscanner;
    other.yyscanner = nullptr;
    location = std::move(other.location);
    continuation_length = other.continuation_length;
    check_for_overlong = other.check_for_overlong;
    seen_carriage_return = other.seen_carriage_return;
    return *this;
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
                    if (continuation_length == 1 && (unsigned char)text[i] < 0xc2) {
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
