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

#undef YY_DECL
#define YY_DECL Parser::symbol_type Lexer::lex(Parser &parser)

#ifndef yyFlexLexerOnce
#  undef yyFlexLexer
#  define yyFlexLexer effectc_impl_Lexer_FlexLexer
#  include <FlexLexer.h>
#endif

#define yylex(lexer) (lexer).lex(*this)

namespace effectc_impl {
}

#include "parser.h"

namespace effectc_impl {
    class Lexer : yyFlexLexer {
    public:
        typedef typename Parser::symbol_type Symbol;
        typedef typename Parser::location_type Location;
        typedef decltype(Location::begin) Position;

        Lexer();

        Parser::symbol_type lex(Parser &parser);
        void advance_location(char *text, int length);

    private:
        Location location;
        uint8_t continuation_length;
        bool check_for_overlong;
        bool seen_carriage_return;
    };
}

#endif // EFFECTC_IMPL_H
