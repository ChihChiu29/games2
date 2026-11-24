#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$DIR/../release/SekiroBrutality.pk3"
mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
(cd "$DIR" && zip -q -r "$OUT" MAPINFO.txt CVARINFO.txt ZSCRIPT.zc)
ls -lh "$OUT"
