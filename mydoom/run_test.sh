#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PB_DIR="$DIR/uzdoompb4/UZDoomPB4"
UZDOOM="$DIR/.bin/uzdoom.AppImage"
PK3="$DIR/release/SekiroBrutality.pk3"

"$DIR/sekiro_pb/build.sh"

export DISPLAY="${DISPLAY:-:0}"
if [ -z "${XAUTHORITY:-}" ]; then
    XAUTH="$(ls "/run/user/$(id -u)"/.mutter-Xwaylandauth.* 2>/dev/null | head -n 1 || true)"
    [ -n "$XAUTH" ] && export XAUTHORITY="$XAUTH"
fi

exec "$UZDOOM" \
    -iwad "$PB_DIR/UZDoom64/DOOM2.WAD" \
    -file \
        "$PB_DIR/Mods/PB.zip" \
        "$PB_DIR/Mods/midi.pk7" \
        "$PB_DIR/Mods/regeneration3.wad" \
        "$PB_DIR/Mods/BEAMFLASHLIGHT.PK3" \
        "$PB_DIR/game.pk3" \
        "$PK3" \
    "$@"
