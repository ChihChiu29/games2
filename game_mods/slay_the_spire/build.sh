#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
JDK="/home/chih/.jdks/jdk-21.0.13.1+2/bin"
mkdir -p build release

"$JDK/javac" --release 8 -cp desktop-1.0.jar -d build src/sts/GoldMod.java
python3 make_patcher.py

# Build patched release/desktop-1.1.jar from clean desktop-1.0.jar
cp -f desktop-1.0.jar release/desktop-1.1.jar
python3 release/patch_sts_jar.py release/desktop-1.1.jar
rm -f release/desktop-1.1.jar.bak

ls -lh release/patch_sts_jar.py release/sts_gold.ini release/desktop-1.1.jar
