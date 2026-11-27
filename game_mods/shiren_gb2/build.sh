#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="$(tr -d '[:space:]' < "$SCRIPT_DIR/VERSION")"
SRC_ROM="$SCRIPT_DIR/shiren2.gbc"
OUT_ROM="$SCRIPT_DIR/release/shiren_gb2_mod${VERSION}.gbc"

if [[ ! -f "$SRC_ROM" ]]; then
    echo "Error: Source ROM not found at $SRC_ROM" >&2
    exit 1
fi

python3 "$SCRIPT_DIR/patch_shiren_gb2.py" "$SRC_ROM" "$OUT_ROM"
