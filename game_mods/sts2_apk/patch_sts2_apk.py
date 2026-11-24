#!/usr/bin/env python3
"""Patch Slay the Spire 2 Android APK (v0.111.0) for 3x or 5x Gold gain.

Verified in assets/payload/SlayTheSpire2.zip -> data_sts2_windows_x86_64/sts2.dll:
  - MethodDef[19658]: MegaCrit.Sts2.Core.Hooks.Hook.ModifyGoldGained
    Original RVA=0x10545C, file_off=0x10365C, FAT hdr=12B, IL=90B, EH=16B (at 0x1036C4)
  - Relocates method into .text trailing zero padding (RVA=0x94FAC0, file_off=0x94DCC0, 328B free)
    and inserts before final `ret` (after `endfinally`, so zero branch/EH offsets change!):
      ldc.i4.s <mult>
      call     System.Decimal::op_Implicit(int32)          ; token 0x0A00116B
      call     System.Decimal::op_Multiply(Decimal,Decimal); token 0x0A0015D9
  - Updates MethodDef[19658].RVA -> 0x94FAC0 and .text VirtualSize -> 0x94DC00.

Usage:
    python3 patch_sts2_apk.py [--mult 3|5]
"""
import argparse
import io
import os
import struct
import sys
import zipfile

sys.path.insert(0, "/home/chih/Workspace/git/games2/game_mods/common")
import apk_v2_sign

SRC_APK = "/home/chih/Workspace/git/games2/game_mods/sts2_apk/STS2-v0.111.0-full-apkvision.apk"
OUT_DIR = "/home/chih/Workspace/git/games2/game_mods/sts2_apk/release"
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


def patch_sts2_dll(dll_bytes: bytes, mult: int) -> bytes:
    dll = bytearray(dll_bytes)

    # 1. Verify original Hook.ModifyGoldGained header, IL tail (06 2a = ldloc.0; ret), and EH
    orig_hdr = bytes(dll[ORIG_METHOD_OFF:ORIG_METHOD_OFF + 12])
    flags, max_stack, code_size, local_sig = struct.unpack("<HHII", orig_hdr)
    assert flags == 0x301B and code_size == ORIG_IL_LEN, f"Header mismatch: {flags:#x}, {code_size}"
    orig_il = bytes(dll[ORIG_METHOD_OFF + 12:ORIG_METHOD_OFF + 12 + ORIG_IL_LEN])
    assert orig_il.endswith(b"\x06\x2a"), f"Unexpected IL tail: {orig_il[-8:].hex()}"
    orig_eh = bytes(dll[ORIG_EH_OFF:ORIG_EH_OFF + ORIG_EH_LEN])
    assert orig_eh.startswith(b"\x01\x10\x00\x00"), f"Unexpected EH header: {orig_eh[:4].hex()}"

    # 2. Build new IL: replace final `2a` (ret) with `ldc.i4.s <mult>; call op_Implicit; call op_Multiply; ret`
    inject = (
        struct.pack("<Bb", 0x1F, mult)              # ldc.i4.s <mult>
        + struct.pack("<BI", 0x28, TOK_DEC_IMPLICIT)
        + struct.pack("<BI", 0x28, TOK_DEC_MULTIPLY)
        + b"\x2a"                                   # ret
    )
    new_il = orig_il[:-1] + inject
    new_il_len = len(new_il)
    pad_len = (4 - (new_il_len % 4)) % 4
    new_hdr = struct.pack("<HHII", flags, max(max_stack, 3), new_il_len, local_sig)
    new_blob = new_hdr + new_il + (b"\x00" * pad_len) + orig_eh

    # 3. Verify free zero padding at NEW_METHOD_OFF
    assert all(b == 0 for b in dll[NEW_METHOD_OFF:NEW_METHOD_OFF + len(new_blob)]), "Target .text padding not zero!"
    dll[NEW_METHOD_OFF:NEW_METHOD_OFF + len(new_blob)] = new_blob

    # 4. Update .text VirtualSize so CLR maps the padding
    pe_off = struct.unpack_from("<I", dll, 0x3C)[0]
    opt_sz = struct.unpack_from("<H", dll, pe_off + 20)[0]
    sec0 = pe_off + 24 + opt_sz
    assert dll[sec0:sec0 + 5] == b".text"
    struct.pack_into("<I", dll, sec0 + 8, 0x94DC00)

    # 5. Update MethodDef[19658].RVA (search for exact original RVA in MethodDef table)
    # MethodDef RVA=0x10545C followed by impl_flags=0x0000, flags=0x0096 (public static)
    needle = struct.pack("<IH", ORIG_METHOD_RVA, 0x0000)
    md_hits = []
    pos = 0x460000
    while True:
        idx = dll.find(needle, pos, 0x760000)
        if idx == -1:
            break
        md_hits.append(idx)
        pos = idx + 1
    assert len(md_hits) == 1, f"Expected 1 MethodDef match for {ORIG_METHOD_RVA:#x}, found {len(md_hits)}"
    struct.pack_into("<I", dll, md_hits[0], NEW_METHOD_RVA)
    print(f"Patched sts2.dll: MethodDef @ {md_hits[0]:#x} RVA {ORIG_METHOD_RVA:#x} -> {NEW_METHOD_RVA:#x} ({mult}x gold, {len(new_blob)}B)")
    return bytes(dll)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mult", type=int, default=3)
    args = ap.parse_args()
    assert 1 <= args.mult <= 127

    os.makedirs(OUT_DIR, exist_ok=True)

    # Extract & patch sts2.dll inside assets/payload/SlayTheSpire2.zip
    print("Reading SlayTheSpire2.zip from APK...")
    patched_payload_path = os.path.join(OUT_DIR, f".tmp_payload_{args.mult}x.zip")
    with zipfile.ZipFile(SRC_APK, "r") as apk_in:
        with apk_in.open(PAYLOAD_ZIP_NAME) as pf:
            with zipfile.ZipFile(pf, "r") as pz_in, zipfile.ZipFile(patched_payload_path, "w") as pz_out:
                for item in pz_in.infolist():
                    data = pz_in.read(item.filename)
                    if item.filename == STS2_DLL_NAME:
                        data = patch_sts2_dll(data, args.mult)
                    zi = zipfile.ZipInfo(item.filename, date_time=item.date_time)
                    zi.compress_type = item.compress_type
                    zi.external_attr = item.external_attr
                    pz_out.writestr(zi, data, compress_type=zi.compress_type)

    with open(patched_payload_path, "rb") as f:
        patched_payload_bytes = f.read()
    os.remove(patched_payload_path)

    unsigned_apk = os.path.join(OUT_DIR, f".tmp_sts2_{args.mult}x.apk")
    signed_apk = os.path.join(OUT_DIR, f"SlayTheSpire2_v0.111.0_{args.mult}xGold.apk")

    print("Repacking 4KB-aligned StS2 APK...")
    apk_v2_sign.repack_apk_aligned(SRC_APK, unsigned_apk, {PAYLOAD_ZIP_NAME: patched_payload_bytes})
    print("Signing with APK Signature Scheme v2...")
    apk_v2_sign.sign_apk_v2(unsigned_apk, signed_apk, KEY_DIR)
    os.remove(unsigned_apk)
    print(f"Done! Wrote: {signed_apk} ({os.path.getsize(signed_apk):,} bytes)")


if __name__ == "__main__":
    main()
