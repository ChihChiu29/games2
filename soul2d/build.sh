#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-0.1.0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${SCRIPT_DIR}/src"
DJGPP_BIN="/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp/bin"
CWSDPMI_EXE="/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp/cwsdpmi/bin/CWSDPMI.EXE"
OUT_DIR="${SCRIPT_DIR}/release"
BUILD_TMP="${SCRIPT_DIR}/build_tmp"
OUT_ZIP="${OUT_DIR}/soul2d_${VERSION}_dospure.zip"

export PATH="${DJGPP_BIN}:${PATH}"

CC_DOS="${DJGPP_BIN}/i586-pc-msdosdjgpp-gcc"
STRIP_DOS="${DJGPP_BIN}/i586-pc-msdosdjgpp-strip"

echo "=== Building Soul2D ${VERSION} for DOS (DJGPP + Lua) ==="

mkdir -p "${OUT_DIR}" "${BUILD_TMP}/objs"

LUA_SRCS=(
  lua/lapi.c lua/lcode.c lua/ldebug.c lua/ldo.c lua/lfunc.c lua/lgc.c
  lua/llex.c lua/lmem.c lua/lobject.c lua/lparser.c lua/lstate.c lua/lstring.c
  lua/ltable.c lua/ltests.c lua/ltm.c lua/lundump.c lua/lvm.c lua/lzio.c
  lua/lauxlib.c lua/lbaselib.c lua/ldblib.c lua/liolib.c lua/lstrlib.c
)

GAME_SRCS=(
  gfx.c
  weapon.c
  character.c
  combat.c
  render_vga.c
  data_lua.c
  main.c
  platform_dos.c
)

OBJS=()
for src in "${LUA_SRCS[@]}"; do
  obj="${BUILD_TMP}/objs/$(echo "${src}" | tr '/' '_').o"
  if [ ! -f "${obj}" ] || [ "${SRC_DIR}/${src}" -nt "${obj}" ]; then
    ${CC_DOS} -O2 -std=gnu89 -I"${SRC_DIR}" -I"${SRC_DIR}/lua" -w -c "${SRC_DIR}/${src}" -o "${obj}"
  fi
  OBJS+=("${obj}")
done

for src in "${GAME_SRCS[@]}"; do
  obj="${BUILD_TMP}/objs/$(echo "${src}" | tr '/' '_').o"
  ${CC_DOS} -O2 -Wall -Wextra -I"${SRC_DIR}" -I"${SRC_DIR}/lua" -c "${SRC_DIR}/${src}" -o "${obj}"
  OBJS+=("${obj}")
done

${CC_DOS} -O2 "${OBJS[@]}" -o "${BUILD_TMP}/SOUL2D.EXE"
${STRIP_DOS} "${BUILD_TMP}/SOUL2D.EXE"

STAGE_DIR="${BUILD_TMP}/stage"
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}"

cp -f "${BUILD_TMP}/SOUL2D.EXE" "${STAGE_DIR}/SOUL2D.EXE"
cp -f "${CWSDPMI_EXE}" "${STAGE_DIR}/CWSDPMI.EXE"
cp -r "${SCRIPT_DIR}/data" "${STAGE_DIR}/data"

cat > "${STAGE_DIR}/DOSBOX.BAT" << 'EOF'
@ECHO OFF
SOUL2D.EXE
EOF

rm -f "${OUT_ZIP}"
(cd "${STAGE_DIR}" && zip -rq "${OUT_ZIP}" .)
rm -rf "${STAGE_DIR}"

echo "=== Successfully built ${OUT_ZIP} ($(du -h "${OUT_ZIP}" | cut -f1)) ==="
