#!/usr/bin/env python3
"""Patch Slay the Spire Android APK (v2.6.1) with:
  - Gold multiplier (default 3x)
  - Starting potion slots (default 5; vanilla 3)
  - Cards drawn per turn (default 6; vanilla 5)

Also replaces the ApkVision Dex2C wrapper in `classes4.dex` (which aborts on
re-signed APKs) with a clean `MyLoaderActivity` + `SpireLicensingServiceCallback`
and signs with official Android SDK `apksigner` (v1 + v2 + v3).

Verified in lib/arm64-v8a/libSpire_ANDROID.so:
  - 0x27248D0 (AbstractPlayer::gainGold):        mov w20, w1         -> add w20, w1, w1, lsl #1 (3x) / #2 (5x)
  - 0x271F230 (AbstractPlayer::<init>):          mov w23, #3         -> mov w23, #<potions>
  - 0x2720088 (AbstractPlayer::initializeClass): ldr w8, [x20, #0x38]-> mov w8, #<draw>

Usage:
    python3 patch_sts_apk.py [--mult 1|2|3|5] [--potions 5] [--draw 6]
"""
import argparse
import glob
import os
import struct
import subprocess
import tempfile
import zipfile

DIR = os.path.dirname(os.path.abspath(__file__))
SRC_APK = os.path.join(DIR, "Slay-the-Spire-v2.6.1-full-apkvision.apk")
OUT_DIR = os.path.join(DIR, "release")
KEY_DIR = "/home/chih/Workspace/git/games2/game_mods/common"

SDK_ROOT = "/home/chih/Android/Sdk"
BUILD_TOOLS = os.path.join(SDK_ROOT, "build-tools/36.1.0")
ANDROID_JAR = os.path.join(SDK_ROOT, "platforms/android-35/android.jar")
JAVA_HOME = "/home/chih/Tools/android-studio/jbr"

SO_NAME = "lib/arm64-v8a/libSpire_ANDROID.so"
GOLD_OFF = 0x27248D0
GOLD_ORIG = bytes.fromhex("f403012a")  # mov w20, w1

MULT_INSNS = {
    1: bytes.fromhex("f403012a"),  # mov w20, w1 (1x)
    2: bytes.fromhex("3400010b"),  # add w20, w1, w1 (w1 * 2)
    3: bytes.fromhex("3404010b"),  # add w20, w1, w1, lsl #1 (w1 * 3)
    5: bytes.fromhex("3408010b"),  # add w20, w1, w1, lsl #2 (w1 * 5)
}

POTION_OFF = 0x271F230
POTION_ORIG = bytes.fromhex("77008052")  # mov w23, #3

DRAW_OFF = 0x2720088
DRAW_ORIG = bytes.fromhex("883a40b9")  # ldr w8, [x20, #0x38]

STUB_INTERFACE_JAVA = """\
package com.google.licensingservicehelper;
import android.app.PendingIntent;
public interface LicensingServiceCallback {
    void allow(String s);
    void dontAllow(PendingIntent pi);
    void applicationError(String s);
}
"""

MY_LOADER_ACTIVITY_JAVA = """\
package com.humble.SlayTheSpire;
import android.app.NativeActivity;
import android.os.Bundle;
public class MyLoaderActivity extends NativeActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }
}
"""

LICENSING_CALLBACK_JAVA = """\
package com.humble.SlayTheSpire;
import android.app.NativeActivity;
import android.app.PendingIntent;
import com.google.licensingservicehelper.LicensingServiceCallback;

class SpireLicensingServiceCallback implements LicensingServiceCallback {
    public static String CACHE_LIC_DATE = "SPIRE_LIC_DATE_CHECKED";
    public static String CACHE_LIC_VAL = "SPIRE_LIC_VALUE";
    private NativeActivity nActivity;

    public SpireLicensingServiceCallback(NativeActivity activity) {
        this.nActivity = activity;
    }

    @Override
    public void allow(String s) {}

    @Override
    public void dontAllow(PendingIntent pi) {}

    @Override
    public void applicationError(String s) {}

    public static void onDestroy() {}
}
"""


def encode_movz_w(rd: int, imm16: int) -> bytes:
    assert 0 <= rd <= 30 and 0 <= imm16 <= 0xFFFF
    return struct.pack("<I", 0x52800000 | (imm16 << 5) | rd)


def _java_env():
    env = os.environ.copy()
    env["JAVA_HOME"] = JAVA_HOME
    env["PATH"] = os.path.join(JAVA_HOME, "bin") + ":" + env.get("PATH", "")
    return env


def build_clean_classes4_dex() -> bytes:
    """Compile clean MyLoaderActivity + SpireLicensingServiceCallback into classes4.dex."""
    env = _java_env()
    with tempfile.TemporaryDirectory() as td:
        src_dir = os.path.join(td, "src")
        cls_dir = os.path.join(td, "cls")
        dex_dir = os.path.join(td, "dex")
        os.makedirs(os.path.join(src_dir, "com/google/licensingservicehelper"), exist_ok=True)
        os.makedirs(os.path.join(src_dir, "com/humble/SlayTheSpire"), exist_ok=True)
        os.makedirs(cls_dir, exist_ok=True)
        os.makedirs(dex_dir, exist_ok=True)

        f_iface = os.path.join(src_dir, "com/google/licensingservicehelper/LicensingServiceCallback.java")
        f_act = os.path.join(src_dir, "com/humble/SlayTheSpire/MyLoaderActivity.java")
        f_cb = os.path.join(src_dir, "com/humble/SlayTheSpire/SpireLicensingServiceCallback.java")
        open(f_iface, "w").write(STUB_INTERFACE_JAVA)
        open(f_act, "w").write(MY_LOADER_ACTIVITY_JAVA)
        open(f_cb, "w").write(LICENSING_CALLBACK_JAVA)

        subprocess.run(
            [
                os.path.join(JAVA_HOME, "bin/javac"),
                "--release", "8",
                "-cp", ANDROID_JAR,
                "-d", cls_dir,
                f_iface, f_act, f_cb,
            ],
            check=True,
            env=env,
        )

        sts_classes = glob.glob(os.path.join(cls_dir, "com/humble/SlayTheSpire/*.class"))
        subprocess.run(
            [
                os.path.join(BUILD_TOOLS, "d8"),
                "--min-api", "21",
                "--lib", ANDROID_JAR,
                "--output", dex_dir,
                *sts_classes,
            ],
            check=True,
            env=env,
        )
        return open(os.path.join(dex_dir, "classes.dex"), "rb").read()


def ensure_keystore() -> str:
    ks = os.path.join(KEY_DIR, "mod_debug.keystore")
    if not os.path.exists(ks):
        os.makedirs(KEY_DIR, exist_ok=True)
        subprocess.run(
            [
                os.path.join(JAVA_HOME, "bin/keytool"),
                "-genkeypair", "-v",
                "-keystore", ks,
                "-storepass", "android",
                "-keypass", "android",
                "-alias", "gamemods",
                "-keyalg", "RSA",
                "-keysize", "2048",
                "-validity", "10000",
                "-dname", "CN=GameMods Debug, O=GameMods, C=US",
            ],
            check=True,
            env=_java_env(),
        )
    return ks


def repack_apk_raw(src_apk: str, out_apk: str, overrides: dict, remove_files: set):
    with zipfile.ZipFile(src_apk, "r") as zin, zipfile.ZipFile(out_apk, "w") as zout:
        for item in zin.infolist():
            fn = item.filename
            if fn in remove_files:
                continue
            if fn.startswith("META-INF/") and (
                fn == "META-INF/MANIFEST.MF" or fn.endswith((".SF", ".RSA", ".DSA", ".EC"))
            ):
                continue
            data = overrides[fn] if fn in overrides else zin.read(fn)
            zi = zipfile.ZipInfo(fn, date_time=item.date_time)
            zi.compress_type = item.compress_type
            zi.external_attr = item.external_attr
            zi.flag_bits = item.flag_bits & ~0x08
            zout.writestr(zi, data, compress_type=zi.compress_type)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mult", type=int, choices=[1, 2, 3, 5], default=3)
    ap.add_argument("--potions", type=int, default=5)
    ap.add_argument("--draw", type=int, default=6)
    args = ap.parse_args()
    assert 1 <= args.potions <= 12 and 1 <= args.draw <= 10

    with open(os.path.join(DIR, "VERSION"), "r", encoding="utf-8") as f:
        version = f.read().strip()

    os.makedirs(OUT_DIR, exist_ok=True)
    with zipfile.ZipFile(SRC_APK, "r") as z:
        so = bytearray(z.read(SO_NAME))

    # 1. Gold multiplier
    cur = bytes(so[GOLD_OFF : GOLD_OFF + 4])
    assert cur == GOLD_ORIG, f"Unexpected bytes at {GOLD_OFF:#x}: {cur.hex()} != {GOLD_ORIG.hex()}"
    so[GOLD_OFF : GOLD_OFF + 4] = MULT_INSNS[args.mult]

    # 2. Starting potion slots
    cur_p = bytes(so[POTION_OFF : POTION_OFF + 4])
    assert cur_p == POTION_ORIG, f"Unexpected bytes at {POTION_OFF:#x}: {cur_p.hex()} != {POTION_ORIG.hex()}"
    so[POTION_OFF : POTION_OFF + 4] = encode_movz_w(23, args.potions)

    # 3. Cards drawn per turn
    cur_d = bytes(so[DRAW_OFF : DRAW_OFF + 4])
    assert cur_d == DRAW_ORIG, f"Unexpected bytes at {DRAW_OFF:#x}: {cur_d.hex()} != {DRAW_ORIG.hex()}"
    so[DRAW_OFF : DRAW_OFF + 4] = encode_movz_w(8, args.draw)

    print(f"Patched {SO_NAME} (v{version}): {args.mult}x gold, {args.potions} potion slots, {args.draw} cards/turn")

    print("Building clean classes4.dex (removing ApkVision Dex2C anti-tamper wrapper)...")
    clean_dex4 = build_clean_classes4_dex()
    print(f"  Built clean classes4.dex ({len(clean_dex4):,} bytes)")

    raw_apk = os.path.join(OUT_DIR, f".tmp_sts1_mod{version}_raw.apk")
    aligned_apk = os.path.join(OUT_DIR, f".tmp_sts1_mod{version}_aligned.apk")
    signed_apk = os.path.join(OUT_DIR, f"SlayTheSpire_v2.6.1_mod{version}.apk")

    remove_files = {
        "lib/arm64-v8a/libAPKVISION.so",
        "lib/arm64-v8a/libapkvisionorg.so",
        "lib/armeabi-v7a/libAPKVISION.so",
        "lib/armeabi-v7a/libapkvisionorg.so",
    }
    print("Repacking APK...")
    repack_apk_raw(SRC_APK, raw_apk, {SO_NAME: bytes(so), "classes4.dex": clean_dex4}, remove_files)

    print("Running official zipalign -p -f 4...")
    subprocess.run([os.path.join(BUILD_TOOLS, "zipalign"), "-p", "-f", "4", raw_apk, aligned_apk], check=True)
    os.remove(raw_apk)

    print("Signing with official apksigner (v1 + v2 + v3)...")
    ks = ensure_keystore()
    subprocess.run(
        [
            os.path.join(BUILD_TOOLS, "apksigner"),
            "sign",
            "--ks", ks,
            "--ks-pass", "pass:android",
            "--key-pass", "pass:android",
            "--ks-key-alias", "gamemods",
            "--v1-signing-enabled", "true",
            "--v2-signing-enabled", "true",
            "--v3-signing-enabled", "true",
            "--out", signed_apk,
            aligned_apk,
        ],
        check=True,
        env=_java_env(),
    )
    os.remove(aligned_apk)

    print("Verifying with apksigner verify --verbose...")
    subprocess.run(
        [os.path.join(BUILD_TOOLS, "apksigner"), "verify", "--verbose", signed_apk],
        check=True,
        env=_java_env(),
    )
    print(f"Done! Wrote & verified: {signed_apk} ({os.path.getsize(signed_apk):,} bytes)")


if __name__ == "__main__":
    main()
