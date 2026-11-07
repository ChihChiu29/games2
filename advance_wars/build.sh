#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="${ROOT_DIR}/aw2bhr"

# Use bundled arm-none-eabi binutils if system toolchain is not in PATH
if ! command -v arm-none-eabi-as >/dev/null 2>&1; then
    export PATH="${PROJECT_DIR}/tools/binutils/bin:${PATH}"
fi

# Ensure agbcc is built
if [ ! -x "${PROJECT_DIR}/tools/agbcc/bin/agbcc" ]; then
    echo "[BUILD] Compiling agbcc toolchain..."
    (cd "${PROJECT_DIR}/tools/agbcc" && ./build.sh && ./install.sh ../..)
fi

JOBS="$(nproc 2>/dev/null || echo 4)"
echo "[BUILD] Building aw2bhr.gba and aw2bhr.sym (${JOBS} jobs)..."
make -C "${PROJECT_DIR}" -j"${JOBS}" aw2bhr.gba aw2bhr.sym

cp -f "${PROJECT_DIR}/aw2bhr.gba" "${ROOT_DIR}/aw2bhr.gba"
echo "[DONE] Built ROM:"
echo "  - ${PROJECT_DIR}/aw2bhr.gba"
echo "  - ${ROOT_DIR}/aw2bhr.gba"
