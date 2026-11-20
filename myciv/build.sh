#!/usr/bin/env bash
# build.sh - Build Civilization (478) via civ1_recom for DOSBox Pure
#
# 1. Compiles civ1_recom for DOS (DJGPP + embedded CWSDSTUB DPMI host) -> CIVR.EXE
# 2. Packages release/civ_<version>_dospure.zip with original DOS game files
#    (original CIV.EXE is kept alongside CIVR.EXE as required), DOS CIV.CFG,
#    and DOSBOX.BAT at the root so DOSBox Pure auto-launches CIVR.EXE.

set -euo pipefail

VERSION="${1:-478}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ORIG_ZIP="${SCRIPT_DIR}/original/civ1_dos.zip"
RECOM_DIR="${SCRIPT_DIR}/civ1_recom"
REL_DIR="${SCRIPT_DIR}/release"
BUILD_TMP="${SCRIPT_DIR}/build_tmp"
STAGE_DIR="${BUILD_TMP}/pkg"
OUT_ZIP="${REL_DIR}/civ_${VERSION}_dospure.zip"

DJGPP_DIR="/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp"
CWSDSTUB_EXE="${DJGPP_DIR}/cwsdpmi/bin/CWSDSTUB.EXE"
CWSDPMI_EXE="${DJGPP_DIR}/cwsdpmi/bin/CWSDPMI.EXE"

if [ ! -f "${ORIG_ZIP}" ]; then
    echo "Error: Could not find ${ORIG_ZIP}" >&2
    exit 1
fi
if [ ! -x "${DJGPP_DIR}/bin/i586-pc-msdosdjgpp-gcc" ]; then
    echo "Error: DJGPP cross-compiler not found at ${DJGPP_DIR}" >&2
    exit 1
fi

export LD_LIBRARY_PATH="${DJGPP_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
JOBS="$(nproc 2>/dev/null || echo 4)"

echo "=== [1/2] Building civ1_recom for DOS (DJGPP, ${JOBS} jobs) ==="
make -C "${RECOM_DIR}" -j"${JOBS}" \
    DJGPP="${DJGPP_DIR}" \
    CWSDSTUB="${CWSDSTUB_EXE}" \
    dos

if [ ! -f "${RECOM_DIR}/civ.exe" ]; then
    echo "Error: Expected ${RECOM_DIR}/civ.exe was not created." >&2
    exit 1
fi

echo "=== [2/2] Packaging DOSBox Pure ZIP (Civilization ${VERSION}) ==="
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}" "${REL_DIR}"

unzip -q -o "${ORIG_ZIP}" -d "${STAGE_DIR}"

# Remove old user save files while keeping CIVIL0.MAP (default Earth map)
rm -f "${STAGE_DIR}"/CIVIL*.SVE "${STAGE_DIR}"/CIVIL[1-9].MAP

# Update version banner in INTRO.TXT and INTRO3.TXT
python3 - "${STAGE_DIR}" "${VERSION}" <<'PYEOF'
import os, sys

stage_dir, version = sys.argv[1], sys.argv[2]
for fname in ("INTRO.TXT", "INTRO3.TXT"):
    path = os.path.join(stage_dir, fname)
    if os.path.exists(path):
        with open(path, "rb") as f:
            data = f.read()
        old_tag = b"Version 475.01"
        new_tag = f"Version {version}".encode("ascii")
        if len(new_tag) < len(old_tag):
            new_tag = new_tag + b" " * (len(old_tag) - len(new_tag))
        data = data.replace(old_tag, new_tag)
        with open(path, "wb") as f:
            f.write(data)
PYEOF

# Install recompiled DOS executable as CIVR.EXE (original CIV.EXE must stay)
cp -f "${RECOM_DIR}/civ.exe" "${STAGE_DIR}/CIVR.EXE"
if [ -f "${CWSDPMI_EXE}" ]; then
    cp -f "${CWSDPMI_EXE}" "${STAGE_DIR}/CWSDPMI.EXE"
fi

# Create DOS-tuned CIV.CFG (320x200 classic VGA mode 13h, wider 160-col map,
# real-time mode available/enabled, no barbarians/rankings, PC speaker + mouse)
cat > "${STAGE_DIR}/CIV.CFG" <<'CFGEOF'
# Civilization (civ1_recom) DOS configuration
gamedir = .
widescreen = 0
mapwidth = 160
realtime = 1
turnsecs = 3
barbarians = 0
rankings = 0
sound = speaker
mouse = 1
CFGEOF

# Create DOSBOX.BAT at root so DOSBox Pure launches CIVR.EXE automatically
printf '@ECHO OFF\r\nCIVR.EXE\r\n' > "${STAGE_DIR}/DOSBOX.BAT"
printf '@ECHO OFF\r\nCIVR.EXE\r\n' > "${STAGE_DIR}/CIV.BAT"

rm -f "${OUT_ZIP}"
(cd "${STAGE_DIR}" && zip -9 -q "${OUT_ZIP}" *)

rm -rf "${BUILD_TMP}"

echo "==> Build complete!"
echo "    DOSBox Pure ZIP: ${OUT_ZIP}"
echo "    DOS Binary:      ${RECOM_DIR}/civ.exe (packaged as CIVR.EXE)"
