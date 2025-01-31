#!/bin/sh
#
# emscripten-patch.sh
#
# This file is part of mkxp.
#
# Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
#
# mkxp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# mkxp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with mkxp.  If not, see <http://www.gnu.org/licenses/>.

# This file looks for all the symbols in the input file that begin with "al" or "alc" (i.e. OpenAL functions) and changes the "a" at the beginning of each symbol into "A".
# This is to work around a symbol conflict with Emscripten's OpenAL implementation by renaming all the symbols in the OpenAL implementation we use.

set -e
for symbol in $(emnm "$1" | cut -d ' ' -f 3 | grep '^alc\?[A-Z]' | awk '{ print length, $0 }' | sort -n -r | cut -d ' ' -f 2)
do
  new_symbol="A$(echo $symbol | cut -c 2-)"
  echo "[emscripten-patch.sh] Replacing '$symbol' with '$new_symbol'"
  sed -i "s/$symbol/$new_symbol/g" "$1"
done
