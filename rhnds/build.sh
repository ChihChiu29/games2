#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d "/home/chih/Workspace/venv/bin" ]; then
    export PATH="/home/chih/Workspace/venv/bin:$PATH"
fi

mkdir -p rhgold/orig release
if [ ! -e rhgold/orig/YLZE01.nds ]; then
    ln -s "$SCRIPT_DIR/rom/rh.nds" rhgold/orig/YLZE01.nds
fi

cd rhgold
python3 tools/configure.py
ninja YLZE01.nds

cp -f YLZE01.nds "$SCRIPT_DIR/release/rh.nds"
echo "Build complete: $SCRIPT_DIR/release/rh.nds"
