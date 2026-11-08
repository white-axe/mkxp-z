/*
 ** effectc/impl.h
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

#ifndef EFFECTC_IMPL_H
#define EFFECTC_IMPL_H

#include <cstdint>

#define YY_DECL effectc_impl::Parser::symbol_type effectc_impl::Lexer::lex(effectc_impl::Parser &parser)
#undef yylex
#define yylex(lexer) (lexer).lex(*this)

#include "parser.hpp"
#ifndef FLEX_SCANNER
#  include "lexer.h"
#endif // FLEX_SCANNER

namespace effectc_impl {
    class Lexer {
    public:
        Lexer();
        ~Lexer();
        Lexer(const Lexer &other) = delete;
        Lexer(Lexer &&other) noexcept;
        Lexer &operator=(const Lexer &other) = delete;
        Lexer &operator=(Lexer &&other) noexcept;

        Parser::symbol_type lex(Parser &parser);
        void advance_location(char *text, int length);

    private:
        yyscan_t yyscanner;
        Parser::location_type location;
        uint8_t continuation_length;
        bool check_for_overlong;
        bool seen_carriage_return;
    };
}

#endif // EFFECTC_IMPL_H
