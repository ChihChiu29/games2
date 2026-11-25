#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
JDK="/home/chih/.jdks/jdk-21.0.13.1+2/bin"
VERSION="$(tr -d '[:space:]' < VERSION)"
mkdir -p build release

# Sync MOD_VERSION in GoldMod.java from VERSION file
sed -i -E "s/(public static final String MOD_VERSION = \")[^\"]+(\";)/\1${VERSION}\2/" src/sts/GoldMod.java

"$JDK/javac" --release 8 -cp desktop-1.0.jar -d build src/sts/GoldMod.java
python3 make_patcher.py

if [ -f desktop-1.0.jar ]; then
    OUT_JAR="release/desktop-${VERSION}.jar"
    cp desktop-1.0.jar "$OUT_JAR"
    python3 release/patch_sts_jar.py "$OUT_JAR"
    rm -f "${OUT_JAR}.bak"
fi

ls -lh release/
