#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-2.3.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_ROOT="${SCRIPT_DIR}/tome_src/src_230"
SRC_DIR="${SRC_ROOT}/src"
DJGPP_BIN="/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp/bin"
CWSDPMI_EXE="/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp/cwsdpmi/bin/CWSDPMI.EXE"
OUT_ZIP="${SCRIPT_DIR}/tome2_${VERSION}_dospure.zip"

export PATH="${DJGPP_BIN}:${PATH}"

CC_DOS="${DJGPP_BIN}/i586-pc-msdosdjgpp-gcc"
STRIP_DOS="${DJGPP_BIN}/i586-pc-msdosdjgpp-strip"
CC_HOST="gcc"

echo "=== Building ToME ${VERSION} for DOS (DJGPP) ==="

cd "${SRC_DIR}"

TOLUA_SRCS=(
  lua/lapi.c lua/lcode.c lua/ldebug.c lua/ldo.c lua/lfunc.c lua/lgc.c
  lua/llex.c lua/lmem.c lua/lobject.c lua/lparser.c lua/lstate.c lua/lstring.c
  lua/ltable.c lua/ltests.c lua/ltm.c lua/lundump.c lua/lvm.c lua/lzio.c
  lua/lauxlib.c lua/lbaselib.c lua/ldblib.c lua/liolib.c lua/lstrlib.c
  lua/tolua_lb.c lua/tolua_rg.c lua/tolua_tt.c lua/tolua_tm.c lua/tolua_gp.c
  lua/tolua_eh.c lua/tolua_bd.c
)

echo "[1/4] Building host tolua generator..."
${CC_HOST} -O2 -DUSE_LUA -I. -I./lua -o tolua_host "${TOLUA_SRCS[@]}" lua/tolua.c lua/tolualua.c -lm

echo "[2/4] Generating Lua C bindings (w_*.c)..."
./tolua_host -n monster  -o w_mnster.c monster.pkg
./tolua_host -n player   -o w_player.c player.pkg
./tolua_host -n player_c -o w_play_c.c player_c.pkg
./tolua_host -n z_pack   -o w_z_pack.c z_pack.pkg
./tolua_host -n object   -o w_obj.c    object.pkg
./tolua_host -n util     -o w_util.c   util.pkg
./tolua_host -n spells   -o w_spells.c spells.pkg
./tolua_host -n quest    -o w_quest.c  quest.pkg
./tolua_host -n dungeon  -o w_dun.c    dungeon.pkg

echo "[3/4] Cross-compiling ToME ${VERSION} with DJGPP..."
GAME_SRCS=(
  main.c main-dos.c main-ibm.c irc.c
  generate.c gen_maze.c gen_evol.c dungeon.c init1.c init2.c plots.c help.c
  store.c birth.c wizard1.c wizard2.c bldg.c cmovie.c
  cmd1.c cmd2.c cmd3.c cmd4.c cmd5.c cmd6.c cmd7.c
  loadsave.c files.c levels.c notes.c squeltch.c
  status.c randart.c skills.c gods.c modules.c
  xtra1.c xtra2.c spells1.c spells2.c melee1.c melee2.c
  object1.c object2.c traps.c monster1.c monster2.c monster3.c
  variable.c tables.c util.c cave.c ghost.c wild.c powers.c
  z-term.c z-rand.c z-form.c z-virt.c z-util.c z-sock.c
  script.c lua_bind.c
  w_util.c w_player.c w_z_pack.c w_obj.c w_mnster.c w_spells.c w_quest.c w_play_c.c w_dun.c
)

CFLAGS_DOS="-O2 -std=gnu89 -DUSE_LUA -DUSE_IBM -I. -I./lua -Wno-implicit-function-declaration -Wno-unused-variable -Wno-unused-but-set-variable"

mkdir -p build_dos

# Compile in parallel batches
PIDS=()
for src in "${GAME_SRCS[@]}" "${TOLUA_SRCS[@]}"; do
  obj="build_dos/$(echo "${src}" | tr '/' '_').o"
  ${CC_DOS} ${CFLAGS_DOS} -c -o "${obj}" "${src}" &
  PIDS+=($!)
  if [ "${#PIDS[@]}" -ge 8 ]; then
    for pid in "${PIDS[@]}"; do wait "${pid}"; done
    PIDS=()
  fi
done
for pid in "${PIDS[@]}"; do wait "${pid}"; done

OBJS_DOS=()
for src in "${GAME_SRCS[@]}" "${TOLUA_SRCS[@]}"; do
  OBJS_DOS+=("build_dos/$(echo "${src}" | tr '/' '_').o")
done

${CC_DOS} ${CFLAGS_DOS} -o "${SRC_ROOT}/TOME.EXE" "${OBJS_DOS[@]}" -lpc
${STRIP_DOS} "${SRC_ROOT}/TOME.EXE"

echo "[4/4] Packaging ${OUT_ZIP}..."
STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGE_DIR}"' EXIT

mkdir -p "${STAGE_DIR}/TOME2"
cp -f "${SRC_ROOT}/TOME.EXE" "${STAGE_DIR}/TOME2/TOME.EXE"
cp -f "${CWSDPMI_EXE}" "${STAGE_DIR}/TOME2/CWSDPMI.EXE"
cp -r "${SRC_ROOT}/lib" "${STAGE_DIR}/TOME2/lib"
mkdir -p "${STAGE_DIR}/TOME2/lib/save" "${STAGE_DIR}/TOME2/lib/user" "${STAGE_DIR}/TOME2/lib/apex" "${STAGE_DIR}/TOME2/lib/bone" "${STAGE_DIR}/TOME2/lib/data"

cat > "${STAGE_DIR}/TOME2/DOSBOX.BAT" << 'EOF'
@ECHO OFF
TOME.EXE
EOF

rm -f "${OUT_ZIP}"
(cd "${STAGE_DIR}/TOME2" && zip -rq "${OUT_ZIP}" .)

echo "=== Successfully built ${OUT_ZIP} ($(du -h "${OUT_ZIP}" | cut -f1)) ==="
