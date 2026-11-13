#!/usr/bin/env bash
# build.sh - Build NetHack 5.0 for DOS and package a zip suitable for DOSBox Pure
# Includes DOSBOX.BAT at the root of the archive so DOSBox Pure launches
# NETHACK.EXE automatically without prompting.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -d "${SCRIPT_DIR}/NetHack/src" ]; then
    NH_DIR="${SCRIPT_DIR}/NetHack"
    OUT_ZIP="${SCRIPT_DIR}/nethack_dospure.zip"
elif [ -d "${SCRIPT_DIR}/src" ]; then
    NH_DIR="${SCRIPT_DIR}"
    OUT_ZIP="${SCRIPT_DIR}/nethack_dospure.zip"
else
    echo "Error: Could not locate NetHack source directory." >&2
    exit 1
fi

cd "${NH_DIR}"

# 1. Fetch DJGPP cross-compiler and dependencies if not already present
if [ ! -x "lib/djgpp/bin/i586-pc-msdosdjgpp-gcc" ]; then
    echo "==> Fetching DJGPP cross-compiler and dependencies..."
    bash sys/msdos/fetch-cross-compiler.sh
fi

# 2. Ensure libfl.so.2 is available if host only has a newer/static flex library
if ! ldd lib/djgpp/bin/i586-pc-msdosdjgpp-ar 2>/dev/null | grep -q "libfl.so.2 => /"; then
    if [ ! -f "lib/djgpp/lib/libfl.so.2" ]; then
        echo "==> Building stub libfl.so.2 for DJGPP binutils..."
        mkdir -p lib/djgpp/lib
        printf 'int yylex(void){return 0;}\nint yywrap(void){return 1;}\n' \
            | gcc -shared -fPIC -Wl,-soname,libfl.so.2 -x c - -o lib/djgpp/lib/libfl.so.2
    fi
fi
export LD_LIBRARY_PATH="${NH_DIR}/lib/djgpp/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

# 3. Generate Makefiles if not yet generated
if ! grep -q "CROSS_TO_MSDOS" Makefile 2>/dev/null; then
    echo "==> Setting up Makefiles..."
    sh sys/unix/setup.sh sys/unix/hints/linux.501
fi

# 4. Build and package DOS zip
echo "==> Building NetHack 5.0 for MS-DOS..."
make -j"$(nproc)" CROSS_TO_MSDOS=1 package

PKG_ZIP="${NH_DIR}/targets/msdos/NH501DOS.ZIP"
if [ ! -f "${PKG_ZIP}" ]; then
    echo "Error: Expected package ${PKG_ZIP} was not created." >&2
    exit 1
fi

# Ensure DOSBOX.BAT and DEFAULTS.NH are at the root of the zip for DOSBox Pure auto-start
printf '@ECHO OFF\r\nNETHACK.EXE\r\n' > "${NH_DIR}/targets/msdos/pkg/DOSBOX.BAT"
cp -f "${NH_DIR}/sys/share/NetHack.cnf" "${NH_DIR}/targets/msdos/pkg/DEFAULTS.NH"
(cd "${NH_DIR}/targets/msdos/pkg" && zip -9 "${PKG_ZIP}" DOSBOX.BAT DEFAULTS.NH >/dev/null)

cp -f "${PKG_ZIP}" "${OUT_ZIP}"

echo "==> Build complete!"
echo "    DOSBox Pure ZIP: ${OUT_ZIP}"
echo "    Target ZIP:      ${PKG_ZIP}"
