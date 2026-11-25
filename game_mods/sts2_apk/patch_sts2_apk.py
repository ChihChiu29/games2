#!/usr/bin/env python3
"""Patch Slay the Spire 2 Android APK (v0.111.0) with:
  - Gold multiplier (default 3x)
  - Starting potion slots (default 5; vanilla 3)
  - Cards drawn per turn (default 6; vanilla 5)

Verified in assets/payload/SlayTheSpire2.zip -> data_sts2_windows_x86_64/sts2.dll:
  - Hook.ModifyGoldGained (MethodDef[19658]): relocated to .text padding 0x94FAC0, multiplies Decimal by <mult>
  - Player.CreateForNewRun @ 0x114D56:        0x19 (ldc.i4.3) -> ldc.i4.<potions> (0x1B = 5)
  - HandDraw caller @ 0x3F5181:               0x1B (ldc.i4.5) -> ldc.i4.<draw>    (0x1C = 6)

Usage:
    python3 patch_sts2_apk.py [--mult 3] [--potions 5] [--draw 6]
"""
import argparse
import os
import struct
import sys
import zipfile

sys.path.insert(0, "/home/chih/Workspace/git/games2/game_mods/common")
import apk_v2_sign

DIR = os.path.dirname(os.path.abspath(__file__))
SRC_APK = os.path.join(DIR, "STS2-v0.111.0-full-apkvision.apk")
OUT_DIR = os.path.join(DIR, "release")
KEY_DIR = "/home/chih/Workspace/git/games2/game_mods/common"

PAYLOAD_ZIP_NAME = "assets/payload/SlayTheSpire2.zip"
STS2_DLL_NAME    = "data_sts2_windows_x86_64/sts2.dll"

ORIG_METHOD_RVA  = 0x10545C
ORIG_METHOD_OFF  = 0x10365C
ORIG_IL_LEN      = 90
ORIG_EH_OFF      = 0x1036C4
ORIG_EH_LEN      = 16

NEW_METHOD_RVA   = 0x94FAC0
NEW_METHOD_OFF   = 0x94DCC0

TOK_DEC_IMPLICIT = 0x0A00116B  # Decimal::op_Implicit(int32)
TOK_DEC_MULTIPLY = 0x0A0015D9  # Decimal::op_Multiply(Decimal, Decimal)

POTION_SLOT_OFF  = 0x114D56    # ldc.i4.3 (0x19) in Player.CreateForNewRun
HAND_DRAW_OFF    = 0x3F5181    # ldc.i4.5 (0x1B) before new Decimal(5) -> Hook.ModifyHandDraw


def ldc_i4_single_byte(n: int) -> int:
    # Single-byte IL opcodes: ldc.i4.0=0x16 .. ldc.i4.8=0x1E
    assert 0 <= n <= 8, f"Single-byte ldc.i4 supports 0..8 (got {n})"
    return 0x16 + n


def patch_sts2_dll(dll_bytes: bytes, mult: int, potions: int, draw: int) -> bytes:
    dll = bytearray(dll_bytes)

    # 1. Gold multiplier in Hook.ModifyGoldGained
    orig_hdr = bytes(dll[ORIG_METHOD_OFF:ORIG_METHOD_OFF + 12])
    flags, max_stack, code_size, local_sig = struct.unpack("<HHII", orig_hdr)
    assert flags == 0x301B and code_size == ORIG_IL_LEN, f"Header mismatch: {flags:#x}, {code_size}"
    orig_il = bytes(dll[ORIG_METHOD_OFF + 12:ORIG_METHOD_OFF + 12 + ORIG_IL_LEN])
    assert orig_il.endswith(b"\x06\x2a"), f"Unexpected IL tail: {orig_il[-8:].hex()}"
    orig_eh = bytes(dll[ORIG_EH_OFF:ORIG_EH_OFF + ORIG_EH_LEN])
    assert orig_eh.startswith(b"\x01\x10\x00\x00"), f"Unexpected EH header: {orig_eh[:4].hex()}"

    inject = (
        struct.pack("<Bb", 0x1F, mult)
        + struct.pack("<BI", 0x28, TOK_DEC_IMPLICIT)
        + struct.pack("<BI", 0x28, TOK_DEC_MULTIPLY)
        + b"\x2a"
    )
    new_il = orig_il[:-1] + inject
    new_il_len = len(new_il)
    pad_len = (4 - (new_il_len % 4)) % 4
    new_hdr = struct.pack("<HHII", flags, max(max_stack, 3), new_il_len, local_sig)
    new_blob = new_hdr + new_il + (b"\x00" * pad_len) + orig_eh

    assert all(b == 0 for b in dll[NEW_METHOD_OFF:NEW_METHOD_OFF + len(new_blob)]), "Target .text padding not zero!"
    dll[NEW_METHOD_OFF:NEW_METHOD_OFF + len(new_blob)] = new_blob

    pe_off = struct.unpack_from("<I", dll, 0x3C)[0]
    opt_sz = struct.unpack_from("<H", dll, pe_off + 20)[0]
    sec0 = pe_off + 24 + opt_sz
    assert dll[sec0:sec0 + 5] == b".text"
    struct.pack_into("<I", dll, sec0 + 8, 0x94DC00)

    needle = struct.pack("<IH", ORIG_METHOD_RVA, 0x0000)
    md_hits = []
    pos = 0x460000
    while True:
        idx = dll.find(needle, pos, 0x760000)
        if idx == -1:
            break
        md_hits.append(idx)
        pos = idx + 1
    assert len(md_hits) == 1
    struct.pack_into("<I", dll, md_hits[0], NEW_METHOD_RVA)

    # 2. Starting potion slots in Player.CreateForNewRun (0x19 = ldc.i4.3)
    assert dll[POTION_SLOT_OFF] == 0x19, f"Expected 0x19 at {POTION_SLOT_OFF:#x}, got {dll[POTION_SLOT_OFF]:#x}"
    dll[POTION_SLOT_OFF] = ldc_i4_single_byte(potions)

    # 3. Cards drawn per turn before Hook.ModifyHandDraw (0x1B = ldc.i4.5)
    assert dll[HAND_DRAW_OFF] == 0x1B, f"Expected 0x1B at {HAND_DRAW_OFF:#x}, got {dll[HAND_DRAW_OFF]:#x}"
    dll[HAND_DRAW_OFF] = ldc_i4_single_byte(draw)

    print(f"Patched sts2.dll: {mult}x gold, {potions} potion slots, {draw} cards/turn")
    return bytes(dll)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mult", type=int, default=3)
    ap.add_argument("--potions", type=int, default=5)
    ap.add_argument("--draw", type=int, default=6)
    args = ap.parse_args()
    assert 1 <= args.mult <= 127 and 1 <= args.potions <= 8 and 1 <= args.draw <= 8

    with open(os.path.join(DIR, "VERSION"), "r", encoding="utf-8") as f:
        version = f.read().strip()

    os.makedirs(OUT_DIR, exist_ok=True)

    print("Reading SlayTheSpire2.zip from APK...")
    patched_payload_path = os.path.join(OUT_DIR, f".tmp_payload_mod{version}.zip")
    with zipfile.ZipFile(SRC_APK, "r") as apk_in:
        with apk_in.open(PAYLOAD_ZIP_NAME) as pf:
            with zipfile.ZipFile(pf, "r") as pz_in, zipfile.ZipFile(patched_payload_path, "w") as pz_out:
                for item in pz_in.infolist():
                    data = pz_in.read(item.filename)
                    if item.filename == STS2_DLL_NAME:
                        data = patch_sts2_dll(data, args.mult, args.potions, args.draw)
                    zi = zipfile.ZipInfo(item.filename, date_time=item.date_time)
                    zi.compress_type = item.compress_type
                    zi.external_attr = item.external_attr
                    pz_out.writestr(zi, data, compress_type=zi.compress_type)

    with open(patched_payload_path, "rb") as f:
        patched_payload_bytes = f.read()
    os.remove(patched_payload_path)

    unsigned_apk = os.path.join(OUT_DIR, f".tmp_sts2_mod{version}.apk")
    signed_apk = os.path.join(OUT_DIR, f"SlayTheSpire2_v0.111.0_mod{version}.apk")

    print("Repacking 4KB-aligned StS2 APK...")
    apk_v2_sign.repack_apk_aligned(SRC_APK, unsigned_apk, {PAYLOAD_ZIP_NAME: patched_payload_bytes})
    print("Signing with APK Signature Scheme v2...")
    apk_v2_sign.sign_apk_v2(unsigned_apk, signed_apk, KEY_DIR)
    os.remove(unsigned_apk)
    print(f"Done! Wrote: {signed_apk} ({os.path.getsize(signed_apk):,} bytes)")


if __name__ == "__main__":
    main()
