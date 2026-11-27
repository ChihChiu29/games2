#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$SCRIPT_DIR/VERSION")"
SRC_ROM="$SCRIPT_DIR/shiren4.nds"
OUT_ROM="$SCRIPT_DIR/release/shiren4_mod${VERSION}.nds"

if [[ ! -f "$SRC_ROM" ]]; then
    echo "Error: Source ROM not found at $SRC_ROM" >&2
    exit 1
fi

python3 "$SCRIPT_DIR/patch_shiren4.py" "$SRC_ROM" "$OUT_ROM"
