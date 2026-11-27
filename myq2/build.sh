#!/usr/bin/env bash
# build.sh - Build MyQ2 (Q2DOS + ACE Bot + Random DM + Slow HP/Ammo Regen) for DOSBox-Pure
#
# 1. Compiles Q2.EXE (static software renderer, SB16/GUS sound, DOS mouse) via DJGPP
# 2. Compiles classic baseq2/gamex86.dxe (Classic SP/DM + slow HP/ammo regen)
# 3. Compiles ace/gamex86.dxe (ACE Bot + Random DM + item shuffle + mutators + regen)
# 4. Packs community DM maps into baseq2/pak3.pak
# 5. Packages release/myq2_<VERSION>_dospure.zip with DOSBOX.BAT auto-launcher

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION="${1:-$(tr -d '[:space:]' < "${SCRIPT_DIR}/VERSION")}"
Q2SRC="${SCRIPT_DIR}/src/q2dos"
PAKS_DIR="${SCRIPT_DIR}/original_paks"
MAPS_DIR="${SCRIPT_DIR}/community_maps"
REL_DIR="${SCRIPT_DIR}/release"
BUILD_TMP="${SCRIPT_DIR}/build_tmp"
STAGE_DIR="${BUILD_TMP}/pkg"
OUT_ZIP="${REL_DIR}/myq2_${VERSION}_dospure.zip"

DJGPP_DIR="/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp"
CWSDPMI_EXE="${DJGPP_DIR}/cwsdpmi/bin/CWSDPMI.EXE"

if [ ! -f "${PAKS_DIR}/pak0.pak" ] || [ ! -f "${PAKS_DIR}/pak1.pak" ]; then
    echo "Error: Missing pak0.pak or pak1.pak in ${PAKS_DIR}" >&2
    exit 1
fi
if [ ! -x "${DJGPP_DIR}/bin/i586-pc-msdosdjgpp-gcc" ]; then
    echo "Error: DJGPP cross-compiler not found at ${DJGPP_DIR}" >&2
    exit 1
fi

export PATH="${DJGPP_DIR}/bin:${DJGPP_DIR}/i586-pc-msdosdjgpp/bin:${PATH}"
export LD_LIBRARY_PATH="${DJGPP_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
export DXE_LD_LIBRARY_PATH="${DJGPP_DIR}/i586-pc-msdosdjgpp/lib"
JOBS="$(nproc 2>/dev/null || echo 4)"
COMMON_CFLAGS="-Wall -O2 -fno-strict-aliasing -fomit-frame-pointer -fcommon"

echo "=== [1/4] Building Q2.EXE (MyQ2 v${VERSION}, static ref_soft, ${JOBS} jobs) ==="
make -C "${Q2SRC}" -j"${JOBS}" -f Makefile.dj \
    CC=i586-pc-msdosdjgpp-gcc \
    CFLAGS="${COMMON_CFLAGS}" \
    REF_DXE=0 REF_STATIC_SOFT=1 \
    USE_WATT32=0 USE_GAMESPY=0 USE_CURL=0 USE_OGG=0 USE_SNDPCI=0 USE_CDAUDIO=0
i586-pc-msdosdjgpp-strip "${Q2SRC}/q2.exe"

echo "=== [2/4] Building baseq2/gamex86.dxe (Classic + Regen) ==="
make -C "${Q2SRC}/game" -j"${JOBS}" -f Makefile.dj \
    CC=i586-pc-msdosdjgpp-gcc \
    CFLAGS="${COMMON_CFLAGS}" \
    DXE3GEN=dxe3gen

echo "=== [3/4] Building ace/gamex86.dxe (ACE Bot + Random DM + Regen) ==="
make -C "${Q2SRC}/acebot" -j"${JOBS}" -f Makefile.dj \
    CC=i586-pc-msdosdjgpp-gcc \
    CFLAGS="${COMMON_CFLAGS}" \
    DXE3GEN=dxe3gen

echo "=== [4/4] Packaging DOSBox-Pure ZIP (MyQ2 v${VERSION}) ==="
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}/baseq2" "${STAGE_DIR}/ace" "${REL_DIR}"

cp -f "${Q2SRC}/q2.exe" "${STAGE_DIR}/Q2.EXE"
if [ -f "${CWSDPMI_EXE}" ]; then
    cp -f "${CWSDPMI_EXE}" "${STAGE_DIR}/CWSDPMI.EXE"
elif [ -f "${Q2SRC}/CWSDPMI.EXE" ]; then
    cp -f "${Q2SRC}/CWSDPMI.EXE" "${STAGE_DIR}/CWSDPMI.EXE"
fi

# Original retail paks
cp -f "${PAKS_DIR}/pak0.pak" "${STAGE_DIR}/baseq2/pak0.pak"
cp -f "${PAKS_DIR}/pak1.pak" "${STAGE_DIR}/baseq2/pak1.pak"
if [ -f "${PAKS_DIR}/pak2.pak" ]; then
    cp -f "${PAKS_DIR}/pak2.pak" "${STAGE_DIR}/baseq2/pak2.pak"
fi

# Pack community DM maps into baseq2/pak3.pak
python3 - "${MAPS_DIR}" "${STAGE_DIR}/baseq2/pak3.pak" <<'PYEOF'
import glob, os, struct, sys

maps_dir, out_pak = sys.argv[1], sys.argv[2]
bsps = sorted(glob.glob(os.path.join(maps_dir, "*.bsp")))
entries = []
offset = 12

with open(out_pak, "wb") as out:
    out.write(b"PACK\x00\x00\x00\x00\x00\x00\x00\x00")
    for path in bsps:
        name = f"maps/{os.path.basename(path)}".encode("ascii")
        assert len(name) < 56, f"Map name too long: {name}"
        with open(path, "rb") as f:
            data = f.read()
        out.write(data)
        entries.append((name, offset, len(data)))
        offset += len(data)
    dirofs = offset
    for name, ofs, length in entries:
        out.write(struct.pack("<56sII", name.ljust(56, b"\x00"), ofs, length))
    dirlen = len(entries) * 64
    out.seek(0)
    out.write(struct.pack("<4sII", b"PACK", dirofs, dirlen))

print(f"Packed {len(entries)} community DM maps into {out_pak}")
PYEOF

# Game DXE modules
cp -f "${Q2SRC}/game/gamex86.dxe" "${STAGE_DIR}/baseq2/gamex86.dxe"
cp -f "${Q2SRC}/acebot/gamex86.dxe" "${STAGE_DIR}/ace/gamex86.dxe"

# Modern WASD + freelook + MyQ2 defaults in baseq2/autoexec.cfg
cat > "${STAGE_DIR}/baseq2/autoexec.cfg" <<'CFGEOF'
// MyQ2 DOSBox-Pure Defaults
set freelook "1"
set lookspring "0"
set lookstrafe "0"
set m_pitch "0.022"
set sensitivity "5"
set crosshair "1"
set cl_run "1"
set vid_ref "soft"
set sw_mode "1"
set viewsize "100"
set fov "90"

// Modern WASD + Mouse controls
bind w "+forward"
bind s "+back"
bind a "+moveleft"
bind d "+moveright"
bind SPACE "+moveup"
bind CTRL "+movedown"
bind MOUSE1 "+attack"
bind MOUSE2 "+moveup"
bind q "weapprev"
bind e "weapnext"
bind r "menu_randomdm"
bind z "menu_zombie"
bind F1 "zm_pick 1"
bind F2 "zm_pick 2"
bind F3 "zm_pick 3"

// MyQ2 defaults (Slow HP & Ammo regen ON, Random Bot DM ready)
set ctf "0"
set myq2_hpregen "1"
set myq2_ammoregen "1"
set myq2_shuffle "1"
set myq2_mutator "1"
set myq2_bots "-1"
set myq2_mappool "0"
set fraglimit "20"

echo "MyQ2 loaded! Press [R] on Main Menu (or Multiplayer -> Random Bot DM) to play!"
CFGEOF

# DOSBOX.BAT auto-launcher for DOSBox-Pure
printf '@ECHO OFF\r\nQ2.EXE +exec autoexec.cfg\r\n' > "${STAGE_DIR}/DOSBOX.BAT"
printf '@ECHO OFF\r\nQ2.EXE +exec autoexec.cfg\r\n' > "${STAGE_DIR}/MYQ2.BAT"

rm -f "${OUT_ZIP}"
(cd "${STAGE_DIR}" && zip -1 -r -q "${OUT_ZIP}" .)

rm -rf "${BUILD_TMP}"

echo "==> Build complete!"
echo "    DOSBox-Pure ZIP: ${OUT_ZIP} ($(du -h "${OUT_ZIP}" | cut -f1))"
echo "    Q2.EXE:          $(ls -lh "${Q2SRC}/q2.exe" | awk '{print $5}')"
echo "    baseq2 DXE:      $(ls -lh "${Q2SRC}/game/gamex86.dxe" | awk '{print $5}')"
echo "    acebot DXE:      $(ls -lh "${Q2SRC}/acebot/gamex86.dxe" | awk '{print $5}')"
