#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROM="${1:-$ROOT_DIR/powder.gba}"

if command -v mgba >/dev/null 2>&1; then
    exec mgba -3 "$ROM"
fi

SYSROOT="/home/chih/.gemini/antigravity-cli/brain/126ba518-2be9-4c55-a9a8-bf1a21337246/scratch/dkp/sysroot"
export LD_LIBRARY_PATH="$SYSROOT/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
exec "$SYSROOT/usr/games/mgba" -3 "$ROM"
