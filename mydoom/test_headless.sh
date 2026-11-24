#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$DIR/sekiro_pb/build.sh"
echo "==> Running headless 8-test combat suite (UZDoom 5.0.3 + Project Brutality 0.4.1 + game.pk3)..."
OUT="$(xvfb-run -a -s "-screen 0 640x480x24" timeout -k 2 45 stdbuf -o0 \
    "$DIR/run_test.sh" -nosound -warp 01 -skill 3 +vid_preferbackend 2 +vid_rendermode 0 \
    "+wait 25; netevent sk_selftest; wait 20; quit" 2>&1 | tr '\r' '\n')"
echo "$OUT" | grep -iE "\[SEKIRO_TEST\]|Script error|VM execution|Abort" || true
if echo "$OUT" | grep -q "SUMMARY: 8/8 PASSED"; then
    echo "PASS: All 8 headless combat tests passed!"
    exit 0
else
    echo "FAIL: Not all tests passed!" >&2
    exit 1
fi
