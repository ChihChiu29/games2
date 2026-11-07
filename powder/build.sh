#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$ROOT_DIR/powder118_src"

export DEVKITPRO="${DEVKITPRO:-/home/chih/.gemini/antigravity-cli/brain/126ba518-2be9-4c55-a9a8-bf1a21337246/scratch/dkp/devkitPro}"
export DEVKITARM="${DEVKITARM:-$DEVKITPRO/devkitARM}"

cd "$SRC_DIR"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "Cleaning GBA build..."
    make -C port/gba clean
    rm -f "$SRC_DIR/powder.gba" "$ROOT_DIR/powder.gba"
    exit 0
fi

# Build support tools and run premake if generated files are missing or "--premake" / "--full" is passed
if [ ! -f "$SRC_DIR/glbdef.h" ] || [ ! -f "$SRC_DIR/rooms/allrooms.cpp" ] || [ ! -f "$SRC_DIR/encyclopedia.cpp" ] || [ "$1" = "--premake" ] || [ "$1" = "--full" ]; then
    echo "Building support tools and running premake..."
    make -C support/bmp2c
    make -C support/encyclopedia2c
    make -C support/enummaker
    make -C support/map2c
    make -C support/txt2c
    make -C support/tile2c
    make -C port/linux premake
else
    # Check if source.txt or encyclopedia.txt were modified more recently than generated outputs
    if [ "$SRC_DIR/source.txt" -nt "$SRC_DIR/glbdef.cpp" ]; then
        echo "Regenerating glbdef.cpp / glbdef.h from source.txt..."
        make -C support/enummaker
        support/enummaker/enummaker source.txt
    fi
    if [ "$SRC_DIR/encyclopedia.txt" -nt "$SRC_DIR/encyclopedia.cpp" ]; then
        echo "Regenerating encyclopedia.cpp / encyclopedia.h from encyclopedia.txt..."
        make -C support/encyclopedia2c
        support/encyclopedia2c/encyclopedia2c encyclopedia.txt
    fi
fi

echo "Building GBA ROM..."
make -C port/gba -j"$(nproc)"
cp -f port/gba/powder.gba "$SRC_DIR/powder.gba"
cp -f port/gba/powder.gba "$ROOT_DIR/powder.gba"

echo "Build complete: $ROOT_DIR/powder.gba"
