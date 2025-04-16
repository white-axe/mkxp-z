/*
** embedtool.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#include <cstdint>
#include <fstream>

int main(int argc, char **argv) {
    if (argc < 4) {
        return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];
    const char *arrayname = argv[3];

    std::ifstream inputf(input, std::ios::binary);
    if (!inputf.is_open()) {
        return 2;
    }

    std::ofstream outputf(output, std::ios::binary);
    if (!outputf.is_open()) {
        return 3;
    }

    outputf << "#include <stddef.h>\n#include <stdint.h>\nextern const uint8_t " << arrayname << "[] = {";

    uint64_t len = 0;
    for (;;) {
        unsigned char c = inputf.get();
        if (inputf.eof()) {
            break;
        }
        outputf << (unsigned int)c << ',';
        ++len;
    }

    outputf << "};\nextern const size_t " << arrayname << "_len = " << len << "ULL;";

    return 0;
}
