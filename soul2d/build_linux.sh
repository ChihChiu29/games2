#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
BUILD_TMP="${SCRIPT_DIR}/build_tmp"
OUT_BIN="${BUILD_TMP}/soul2d_linux"

echo "=== Building Soul2D for Linux Terminal (GCC + Lua) ==="

mkdir -p "${BUILD_TMP}/objs_linux"

LUA_SRCS=(
  lua/lapi.c lua/lcode.c lua/ldebug.c lua/ldo.c lua/lfunc.c lua/lgc.c
  lua/llex.c lua/lmem.c lua/lobject.c lua/lparser.c lua/lstate.c lua/lstring.c
  lua/ltable.c lua/ltests.c lua/ltm.c lua/lundump.c lua/lvm.c lua/lzio.c
  lua/lauxlib.c lua/lbaselib.c lua/ldblib.c lua/liolib.c lua/lstrlib.c
)

GAME_SRCS=(
  weapon.c
  character.c
  combat.c
  render.c
  data_lua.c
  main.c
  platform_linux.c
)

OBJS=()
for src in "${LUA_SRCS[@]}"; do
  obj="${BUILD_TMP}/objs_linux/$(echo "${src}" | tr '/' '_').o"
  if [ ! -f "${obj}" ] || [ "${SRC_DIR}/${src}" -nt "${obj}" ]; then
    gcc -O2 -std=gnu89 -I"${SRC_DIR}" -I"${SRC_DIR}/lua" -w -c "${SRC_DIR}/${src}" -o "${obj}"
  fi
  OBJS+=("${obj}")
done

for src in "${GAME_SRCS[@]}"; do
  obj="${BUILD_TMP}/objs_linux/$(echo "${src}" | tr '/' '_').o"
  gcc -O2 -Wall -Wextra -I"${SRC_DIR}" -I"${SRC_DIR}/lua" -c "${SRC_DIR}/${src}" -o "${obj}"
  OBJS+=("${obj}")
done

gcc -O2 "${OBJS[@]}" -o "${OUT_BIN}"

echo "=== Successfully built ${OUT_BIN} ==="
echo "Run from ${SCRIPT_DIR} with: ./build_tmp/soul2d_linux"
