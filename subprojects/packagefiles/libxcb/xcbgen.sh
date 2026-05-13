#!/bin/sh
set -ex

python="$1"
c_client="$2"
private_dir="$3"
outdir="$4"
root="$5"
name="$6"
shift 6

c_client="$(realpath "$c_client")"
outdir="$(realpath "$outdir")"
cd "$private_dir"
"$python" "$c_client" -c libxcb -l 'X Version 11' -s 3 -p "$root" "$root/src/$name.xml"
mv "$name.h" "$outdir/$name.h"
