#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${1:-}" == "--dummy" ]]; then
    shift
    exec env SDL_VIDEODRIVER=dummy "$DIR/eden-cli" "$@"
else
    exec xvfb-run -a -s "-screen 0 1280x720x24" "$DIR/eden-cli" "$@"
fi
