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
    #include <cerrno>
    #include <climits>
    #include <clocale>
    #include <cmath>
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

    static Parser::symbol_type parse_integer(Parser &parser, const char *str, int base, uint64_t maximum, const Parser::location_type &location) {
        static_assert(UINT64_MAX <= ULLONG_MAX, "uint64_t should be able to fit into unsigned long long, but it doesn't");
        int old_errno = errno;
        errno = 0;
        unsigned long long value = std::strtoull(str, nullptr, base);
        if (errno != 0) {
            errno = old_errno;
            ERROR("integer literal out of range");
        }
        errno = old_errno;
        if (value > maximum) {
            ERROR("integer literal out of range");
        }
        return parser.make_integer_literal((uint64_t)value, location);
    }

    static Parser::symbol_type parse_floating_point(Parser &parser, char *str, const Parser::location_type &location) {
        int old_errno = errno;
        errno = 0;
        std::lconv *lconv = std::localeconv();
        char *ptr = std::strpbrk(str, ".");
        if (ptr != nullptr) {
            *ptr = *lconv->decimal_point;
        }
        double value = std::strtod(str, nullptr);
        if (ptr != nullptr) {
            *ptr = '.';
        }
        if (errno != 0) {
            value = (double)INFINITY;
        }
        errno = old_errno;
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
    return parse_integer(parser, str, 16, ptr != nullptr ? UINT64_MAX : UINT32_MAX, location);
}

0[0-7]*([uU][lL]?|[lL][uU]?)? { // Octal integer literal
    char *str = yytext + 1;
    char *ptr = std::strpbrk(str, "uUlL");
    if (ptr != nullptr) {
        *ptr = 0;
    }
    return parse_integer(parser, str, 8, ptr != nullptr ? UINT64_MAX : UINT32_MAX, location);
}

[1-9][0-9]*([uU][lL]?|[lL][uU]?)? { // Decimal integer literal
    char *ptr = std::strpbrk(yytext, "uUlL");
    if (ptr != nullptr) {
        *ptr = 0;
    }
    return parse_integer(parser, yytext, 10, UINT64_MAX, location);
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

AppendStructuredBuffer {
    return parser.make_keyword_append_structured_buffer(location);
}

asm {
    return parser.make_keyword_asm(location);
}

asm_fragment {
    return parser.make_keyword_asm_fragment(location);
}

auto {
    return parser.make_keyword_auto(location);
}

BlendState {
    return parser.make_keyword_blend_state(location);
}

break {
    return parser.make_keyword_break(location);
}

Buffer {
    return parser.make_keyword_buffer(location);
}

ByteAddressBuffer {
    return parser.make_keyword_byte_address_buffer(location);
}

case {
    return parser.make_keyword_case(location);
}

catch {
    return parser.make_keyword_catch(location);
}

cbuffer {
    return parser.make_keyword_cbuffer(location);
}

centroid {
    return parser.make_keyword_centroid(location);
}

class {
    return parser.make_keyword_class(location);
}

column_major {
    return parser.make_keyword_column_major(location);
}

compile {
    return parser.make_keyword_compile(location);
}

compile_fragment {
    return parser.make_keyword_compile_fragment(location);
}

CompileShader {
    return parser.make_keyword_compile_shader(location);
}

ComputeShader {
    return parser.make_keyword_compute_shader(location);
}

const {
    return parser.make_keyword_const(location);
}

const_cast {
    return parser.make_keyword_const_cast(location);
}

ConsumeStructuredBuffer {
    return parser.make_keyword_consume_structured_buffer(location);
}

continue {
    return parser.make_keyword_continue(location);
}

default {
    return parser.make_keyword_default(location);
}

delete {
    return parser.make_keyword_delete(location);
}

DepthStencilState {
    return parser.make_keyword_depth_stencil_state(location);
}

DepthStencilView {
    return parser.make_keyword_depth_stencil_view(location);
}

discard {
    return parser.make_keyword_discard(location);
}

do {
    return parser.make_keyword_do(location);
}

DomainShader {
    return parser.make_keyword_domain_shader(location);
}

dynamic_cast {
    return parser.make_keyword_dynamic_cast(location);
}

else {
    return parser.make_keyword_else(location);
}

enum {
    return parser.make_keyword_enum(location);
}

explicit {
    return parser.make_keyword_explicit(location);
}

export {
    return parser.make_keyword_export(location);
}

extern {
    return parser.make_keyword_extern(location);
}

false {
    return parser.make_keyword_false(location);
}

for {
    return parser.make_keyword_for(location);
}

friend {
    return parser.make_keyword_friend(location);
}

fxgroup {
    return parser.make_keyword_fxgroup(location);
}

GeometryShader {
    return parser.make_keyword_geometry_shader(location);
}

goto {
    return parser.make_keyword_goto(location);
}

groupshared {
    return parser.make_keyword_groupshared(location);
}

if {
    return parser.make_keyword_if(location);
}

in {
    return parser.make_keyword_in(location);
}

inline {
    return parser.make_keyword_inline(location);
}

inout {
    return parser.make_keyword_inout(location);
}

InputPatch {
    return parser.make_keyword_input_patch(location);
}

interface {
    return parser.make_keyword_interface(location);
}

line {
    return parser.make_keyword_line(location);
}

lineadj {
    return parser.make_keyword_lineadj(location);
}

linear {
    return parser.make_keyword_linear(location);
}

LineStream {
    return parser.make_keyword_line_stream(location);
}

matrix {
    return parser.make_keyword_matrix(location);
}

mutable {
    return parser.make_keyword_mutable(location);
}

namespace {
    return parser.make_keyword_namespace(location);
}

new {
    return parser.make_keyword_new(location);
}

nointerpolation {
    return parser.make_keyword_nointerpolation(location);
}

noperspective {
    return parser.make_keyword_noperspective(location);
}

NULL {
    return parser.make_keyword_null(location);
}

operator {
    return parser.make_keyword_operator(location);
}

out {
    return parser.make_keyword_out(location);
}

OutputPatch {
    return parser.make_keyword_output_patch(location);
}

packoffset {
    return parser.make_keyword_packoffset(location);
}

pass {
    return parser.make_keyword_pass(location);
}

pixelfragment {
    return parser.make_keyword_pixelfragment(location);
}

PixelShader {
    return parser.make_keyword_pixel_shader(location);
}

point {
    return parser.make_keyword_point(location);
}

PointStream {
    return parser.make_keyword_point_stream(location);
}

precise {
    return parser.make_keyword_precise(location);
}

private {
    return parser.make_keyword_private(location);
}

protected {
    return parser.make_keyword_protected(location);
}

public {
    return parser.make_keyword_public(location);
}

RasterizerState {
    return parser.make_keyword_rasterizer_state(location);
}

register {
    return parser.make_keyword_register(location);
}

reinterpret_cast {
    return parser.make_keyword_reinterpret_cast(location);
}

RenderTargetView {
    return parser.make_keyword_render_target_view(location);
}

return {
    return parser.make_keyword_return(location);
}

row_major {
    return parser.make_keyword_row_major(location);
}

RWBuffer {
    return parser.make_keyword_rw_buffer(location);
}

RWByteAddressBuffer {
    return parser.make_keyword_rw_byte_address_buffer(location);
}

RWStructuredBuffer {
    return parser.make_keyword_rw_structured_buffer(location);
}

RWTexture1D {
    return parser.make_keyword_rw_texture_1d(location);
}

RWTexture1DArray {
    return parser.make_keyword_rw_texture_1d_array(location);
}

RWTexture2D {
    return parser.make_keyword_rw_texture_2d(location);
}

RWTexture2DArray {
    return parser.make_keyword_rw_texture_2d_array(location);
}

RWTexture3D {
    return parser.make_keyword_rw_texture_3d(location);
}

sample {
    return parser.make_keyword_sample(location);
}

sampler {
    return parser.make_keyword_sampler(location);
}

sampler1D {
    return parser.make_keyword_sampler_2d(location);
}

sampler2D {
    return parser.make_keyword_sampler_2d(location);
}

sampler3D {
    return parser.make_keyword_sampler_3d(location);
}

SamplerComparisonState {
    return parser.make_keyword_sampler_comparison_state(location);
}

samplerCUBE {
    return parser.make_keyword_sampler_cube(location);
}

sampler_state {
    return parser.make_keyword_sampler_state(location);
}

SamplerState {
    return parser.make_keyword_sampler_state_2(location);
}

shared {
    return parser.make_keyword_shared(location);
}

signed {
    return parser.make_keyword_signed(location);
}

sizeof {
    return parser.make_keyword_sizeof(location);
}

snorm {
    return parser.make_keyword_snorm(location);
}

stateblock {
    return parser.make_keyword_stateblock(location);
}

stateblock_state {
    return parser.make_keyword_stateblock_state(location);
}

static {
    return parser.make_keyword_static(location);
}

static_cast {
    return parser.make_keyword_static_cast(location);
}

string {
    return parser.make_keyword_string(location);
}

String {
    return parser.make_keyword_string_2(location);
}

struct {
    return parser.make_keyword_struct(location);
}

StructuredBuffer {
    return parser.make_keyword_structured_buffer(location);
}

switch {
    return parser.make_keyword_switch(location);
}

tbuffer {
    return parser.make_keyword_tbuffer(location);
}

technique {
    return parser.make_keyword_technique(location);
}

technique10 {
    return parser.make_keyword_technique10(location);
}

technique11 {
    return parser.make_keyword_technique11(location);
}

template {
    return parser.make_keyword_template(location);
}

texture {
    return parser.make_keyword_texture(location);
}

Texture1D {
    return parser.make_keyword_texture_1d(location);
}

Texture1DArray {
    return parser.make_keyword_texture_1d_array(location);
}

Texture2D {
    return parser.make_keyword_texture_2d(location);
}

Texture2DArray {
    return parser.make_keyword_texture_2d_array(location);
}

Texture2DMS {
    return parser.make_keyword_texture_2d_ms(location);
}

Texture2DMSArray {
    return parser.make_keyword_texture_2d_ms_array(location);
}

Texture3D {
    return parser.make_keyword_texture_3d(location);
}

TextureCube {
    return parser.make_keyword_texture_cube(location);
}

TextureCubeArray {
    return parser.make_keyword_texture_cube_array(location);
}

this {
    return parser.make_keyword_this(location);
}

throw {
    return parser.make_keyword_throw(location);
}

triangle {
    return parser.make_keyword_triangle(location);
}

triangleadj {
    return parser.make_keyword_triangleadj(location);
}

TriangleStream {
    return parser.make_keyword_triangle_stream(location);
}

true {
    return parser.make_keyword_true(location);
}

try {
    return parser.make_keyword_try(location);
}

typedef {
    return parser.make_keyword_typedef(location);
}

typename {
    return parser.make_keyword_typename(location);
}

uniform {
    return parser.make_keyword_uniform(location);
}

union {
    return parser.make_keyword_union(location);
}

unorm {
    return parser.make_keyword_unorm(location);
}

unsigned {
    return parser.make_keyword_unsigned(location);
}

using {
    return parser.make_keyword_using(location);
}

vector {
    return parser.make_keyword_vector(location);
}

vertexfragment {
    return parser.make_keyword_vertexfragment(location);
}

VertexShader {
    return parser.make_keyword_vertex_shader(location);
}

virtual {
    return parser.make_keyword_virtual(location);
}

void {
    return parser.make_keyword_void(location);
}

volatile {
    return parser.make_keyword_volatile(location);
}

while {
    return parser.make_keyword_while(location);
}

[A-Za-z_]* { // Identifier or type name
    // TODO: check if yytext is an identifier or type name
    return parser.make_identifier(yytext, location);
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
    if (yylex_init(&yyscanner) != 0) {
        throw std::bad_alloc();
    }
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
