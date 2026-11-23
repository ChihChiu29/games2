#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p release

python3 -m ziglang cc \
    -target x86_64-windows-gnu \
    -shared -O2 -Wall -Wextra \
    src/dinput8.c src/dinput8.def \
    -o release/dinput8.dll \
    -lkernel32 -luser32

ls -lh release/dinput8.dll
file release/dinput8.dll
