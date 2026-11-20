#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -d "/home/chih/Workspace/venv/bin" ]; then
    export PATH="/home/chih/Workspace/venv/bin:$PATH"
fi

DKP_DIR="${SCRIPT_DIR}/.devkitpro"
CACHE_DIR="${SCRIPT_DIR}/.cache"
VPNX_DIR="${SCRIPT_DIR}/vpnx"
RELEASE_DIR="${SCRIPT_DIR}/release"
BUILD_TS="$(date +%Y%m%d_%H%M%S)"
OUT_NSP="${RELEASE_DIR}/vpnx_${BUILD_TS}.nsp"

mkdir -p "$DKP_DIR" "$CACHE_DIR" "$RELEASE_DIR"

# 1. Ensure local devkitA64 + libnx + switch-tools toolchain is present
if [ ! -x "${DKP_DIR}/devkitA64/bin/aarch64-none-elf-gcc" ] || [ ! -x "${DKP_DIR}/tools/bin/elf2nso" ]; then
    echo "==> Setting up local devkitPro (devkitA64 + libnx + switch-tools)..."
    for url in \
        "https://wii.leseratte10.de/devkitPro/devkitA64/r29%20%282026-01%29/devkita64-binutils-2.45.1-2-linux_x86_64.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/devkitA64/r29%20%282026-01%29/devkita64-gcc-15.2.0-7-linux_x86_64.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/devkitA64/r29%20%282026-01%29/devkita64-newlib-4.6.0.20260123-4-any.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/libnx/libnx-4.12.0-1-any.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/switch/switch-tools-1.13.1-1-linux_x86_64.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/cmake/dkp-cmake-common-utils-1.5.3-1-any.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/cmake/switch-cmake-1.5.2-1-any.pkg.tar.zst" \
        "https://wii.leseratte10.de/devkitPro/cmake/devkita64-cmake-1.1.3-1-any.pkg.tar.zst"; do
        echo "  Fetching $(basename "$url")..."
        curl -sL "$url" | tar --zstd -xf - -C "$DKP_DIR" --strip-components=2 --wildcards "opt/devkitpro/*"
    done
fi

export DEVKITPRO="$DKP_DIR"

# 2. Build the NSP application payload (forwarder/main.c -> hbl.nso, hbl.npdm, default.nacp)
echo "==> Compiling NSP application loader (hbl.nso, hbl.npdm, default.nacp)..."
make -C "${VPNX_DIR}/forwarder" clean all
cp -f "${VPNX_DIR}/docs/brand/icon.jpg" "${VPNX_DIR}/romfs/forwarder/icon.jpg"

# 3. Prepare VPNX NRO and runtime assets
if [ "${BUILD_FROM_SOURCE:-0}" = "1" ]; then
    echo "==> Building VPNX NRO from source..."
    (
        cd "$VPNX_DIR"
        ./build.sh
        ./build-sdl3.sh
        ./build-sdl3-extras.sh
        ./build-bgfx.sh
        ./build-deps.sh
        VARIANT="${VARIANT:-gl}" ./build-vpinball.sh
    )
fi

STAGE_DIR="$(mktemp -d -t vpnx_romfs_XXXXXX)"
trap 'rm -rf "$STAGE_DIR"' EXIT

mkdir -p "${STAGE_DIR}/forwarder" "${STAGE_DIR}/switch/vpnx"
cp -r "${VPNX_DIR}/romfs/forwarder/"* "${STAGE_DIR}/forwarder/"

if [ -f "${VPNX_DIR}/build/vpinball-vk.nro" ] && [ -d "${VPNX_DIR}/vpinball/src/assets" ]; then
    echo "==> Using locally built vpinball-vk.nro..."
    cp -f "${VPNX_DIR}/build/vpinball-vk.nro" "${STAGE_DIR}/switch/vpnx/vpnx.nro"
    cp -r "${VPNX_DIR}/vpinball/src/assets" "${STAGE_DIR}/switch/vpnx/assets"
    cp -r "${VPNX_DIR}/vpinball/scripts" "${STAGE_DIR}/switch/vpnx/scripts"
    mkdir -p "${STAGE_DIR}/switch/vpnx/plugins/scoreview"
    cp -r "${VPNX_DIR}/vpinball/plugins/scoreview/layouts" "${STAGE_DIR}/switch/vpnx/plugins/scoreview/layouts"
    cp -f "${VPNX_DIR}/vpinball/plugins/scoreview/plugin.cfg" "${STAGE_DIR}/switch/vpnx/plugins/scoreview/plugin.cfg"
    find "${STAGE_DIR}/switch/vpnx/assets" -maxdepth 1 -name '*.vpx' -delete
elif [ -f "${VPNX_DIR}/build/vpinball-gl.nro" ] && [ -d "${VPNX_DIR}/vpinball/src/assets" ]; then
    echo "==> Using locally built vpinball-gl.nro..."
    cp -f "${VPNX_DIR}/build/vpinball-gl.nro" "${STAGE_DIR}/switch/vpnx/vpnx.nro"
    cp -r "${VPNX_DIR}/vpinball/src/assets" "${STAGE_DIR}/switch/vpnx/assets"
    cp -r "${VPNX_DIR}/vpinball/scripts" "${STAGE_DIR}/switch/vpnx/scripts"
    mkdir -p "${STAGE_DIR}/switch/vpnx/plugins/scoreview"
    cp -r "${VPNX_DIR}/vpinball/plugins/scoreview/layouts" "${STAGE_DIR}/switch/vpnx/plugins/scoreview/layouts"
    cp -f "${VPNX_DIR}/vpinball/plugins/scoreview/plugin.cfg" "${STAGE_DIR}/switch/vpnx/plugins/scoreview/plugin.cfg"
    find "${STAGE_DIR}/switch/vpnx/assets" -maxdepth 1 -name '*.vpx' -delete
else
    REL_ZIP="${CACHE_DIR}/vpnx-0.1.0.zip"
    if [ ! -f "$REL_ZIP" ]; then
        if [ -f "/tmp/vpnx-0.1.0.zip" ]; then
            cp -f "/tmp/vpnx-0.1.0.zip" "$REL_ZIP"
        else
            echo "==> Downloading VPNX 0.1.0 Vulkan runtime package..."
            curl -sL "https://github.com/RumaTeam/vpnx/releases/download/vpnx-0.1.0/vpnx-0.1.0.zip" -o "$REL_ZIP"
        fi
    fi
    echo "==> Extracting VPNX Vulkan runtime (vpnx.nro, assets, scripts, scoreview)..."
    unzip -q -o "$REL_ZIP" -d "$STAGE_DIR"
fi

# 4. Bundle all tables from tables/ (and table/ if present)
TABLE_COUNT=0
for TDIR in "${SCRIPT_DIR}/tables" "${SCRIPT_DIR}/table"; do
    [ -d "$TDIR" ] || continue
    echo "==> Bundling tables from ${TDIR}..."
    shopt -s nullglob
    for item in "$TDIR"/*; do
        base="$(basename "$item")"
        case "$base" in
            *.vpxz|*.zip)
                echo "  Extracting table archive: $base"
                unzip -q -o "$item" -d "${STAGE_DIR}/switch/vpnx"
                TABLE_COUNT=$((TABLE_COUNT + 1))
                ;;
            *.vpx)
                echo "  Copying table file: $base"
                cp -f "$item" "${STAGE_DIR}/switch/vpnx/"
                TABLE_COUNT=$((TABLE_COUNT + 1))
                ;;
            *)
                if [ -d "$item" ]; then
                    echo "  Copying table directory: $base"
                    cp -r "$item" "${STAGE_DIR}/switch/vpnx/"
                    TABLE_COUNT=$((TABLE_COUNT + 1))
                fi
                ;;
        esac
    done
    shopt -u nullglob
done

find "$STAGE_DIR" -name '.DS_Store' -delete
echo "==> Bundled ${TABLE_COUNT} table package(s)."

# 5. Build the final .nsp package under release/ and remove previous builds
python3 "${SCRIPT_DIR}/build/build_nsp.py" "$DKP_DIR" "$VPNX_DIR" "$STAGE_DIR" "$OUT_NSP"
find "$RELEASE_DIR" -maxdepth 1 -type f -name '*.nsp' ! -path "$OUT_NSP" -delete
