#!/usr/bin/env python3
"""
Shiren the Wanderer 6 Plus (Android APK, com.spikechunsoft.shirenthewanderer6plus.ww) Patcher
Patches lib/arm64-v8a/libil2cpp.so (Unity IL2CPP v31, ARM64) with:
  1. 3x Gold (Gitan) earn: inline in UserState::AddGitan (.bsf::plm @ 0x031F0528),
     replaces redundant duplicate static-init block at 0x031F0578..0x031F05A7 so
     positive Gitan deltas (w20 > 0) are multiplied by 3x while spending (w20 <= 0)
     is untouched.
  2. 3x Backpack Size (default 72 slots; vanilla 24):
     - ItemBag..ctor (0x033D151C): forces capacity w1 = 72 before branching to base ctor.
     - ItemBag::uoe Deserialize (0x033D1514): jumps to 28-byte helper at 0x031F0588
       that calls ItemContainer::Deserialize (0x0346B8C4) and upgrades [this+0x18]
       and [this+0x1C] to 72 so existing saves automatically expand.
  3. Repacks 4KB-aligned APK and signs with APK Signature Scheme v2.
"""

import argparse
import os
import struct
import sys
import zipfile

sys.path.insert(0, "/home/chih/Workspace/git/games2/game_mods/common")
import apk_v2_sign

DIR = os.path.dirname(os.path.abspath(__file__))
SRC_APK = os.path.join(DIR, "com.spikechunsoft.shirenthewanderer6plus.ww.apk")
OUT_DIR = os.path.join(DIR, "release")
KEY_DIR = "/home/chih/Workspace/git/games2/game_mods/common"
IL2CPP_ENTRY = "lib/arm64-v8a/libil2cpp.so"
VA_DELTA = 0x4000  # PT_LOAD[1]: va = file_off + 0x4000


def enc_b(src_va: int, dst_va: int, link: bool = False) -> int:
    rel = dst_va - src_va
    assert (rel & 3) == 0 and -(1 << 27) <= rel < (1 << 27), f"Branch out of range: {src_va:#x} -> {dst_va:#x}"
    imm26 = (rel >> 2) & 0x03FFFFFF
    return (0x94000000 if link else 0x14000000) | imm26


def patch_va(buf: bytearray, va: int, expected: bytes, replacement: bytes, desc: str):
    off = va - VA_DELTA
    actual = bytes(buf[off : off + len(expected)])
    if actual != expected:
        raise RuntimeError(f"{desc} @ VA {va:#x} (file {off:#x}): expected {expected.hex()}, got {actual.hex()}")
    buf[off : off + len(replacement)] = replacement
    print(f"  [OK] {desc} @ VA {va:#x}")


def patch_libil2cpp(so_bytes: bytes, gold_mult: int, bag_size: int) -> bytes:
    so = bytearray(so_bytes)

    # Expected redundant static-init block in .bsf::plm (0x031F0578..0x031F05A7, 48 bytes)
    orig_dead_block = bytes.fromhex(
        "f70602d0e8365439c800003560e70190006c46f9048ce89728008052e8361439a00240f908e440b9680000355d8ce897"
    )
    new_block = bytearray(48)

    # [1] 3x Gold inline at 0x031F0578..0x031F0587 (16 bytes)
    if gold_mult == 3:
        scale_insn = 0x0B140694  # add w20, w20, w20, lsl #1  (w20 * 3)
    elif gold_mult == 2:
        scale_insn = 0x0B140294  # add w20, w20, w20          (w20 * 2)
    elif gold_mult == 1:
        scale_insn = 0xD503201F  # nop
    else:
        raise ValueError("Shiren 6 --gold-mult supports 1, 2, or 3")

    struct.pack_into(
        "<4I",
        new_block,
        0,
        0x7100029F,                    # 031f0578: cmp  w20, #0
        0x5400004D,                    # 031f057c: b.le #0x031f0584
        scale_insn,                    # 031f0580: scale positive delta
        enc_b(0x031F0584, 0x031F05A8), # 031f0584: b    #0x031f05a8
    )

    # [2] ItemBag::uoe Deserialize auto-upgrade helper at 0x031F0588..0x031F05A3 (28 bytes)
    if bag_size > 24:
        mov_w8 = 0x52800008 | ((bag_size & 0xFFFF) << 5)
        struct.pack_into(
            "<7I",
            new_block,
            16,
            0xA9BF7BF3,                          # 031f0588: stp x19, x30, [sp, #-0x10]!
            0xAA0003F3,                          # 031f058c: mov x19, x0
            enc_b(0x031F0590, 0x0346B8C4, True), # 031f0590: bl  #0x0346b8c4 (ItemContainer::Deserialize)
            mov_w8,                              # 031f0594: mov w8, #bag_size
            0x29032268,                          # 031f0598: stp w8, w8, [x19, #0x18]
            0xA8C17BF3,                          # 031f059c: ldp x19, x30, [sp], #0x10
            0xD65F03C0,                          # 031f05a0: ret
        )
        struct.pack_into("<I", new_block, 44, 0xD503201F)  # 031f05a4: nop
    else:
        for o in range(16, 48, 4):
            struct.pack_into("<I", new_block, o, 0xD503201F)

    patch_va(
        so,
        0x031F0578,
        orig_dead_block,
        bytes(new_block),
        f"Install {gold_mult}x Gold inline + ItemBag Deserialize helper (bag={bag_size})",
    )

    if bag_size > 24:
        # ItemBag::uoe (0x033D1514): mov x2, xzr; b #0x031F0588
        patch_va(
            so,
            0x033D1514,
            bytes.fromhex("e2031faaeb680214"),  # mov x2, xzr; b #0x0346b8c4
            struct.pack("<2I", 0xAA1F03E2, enc_b(0x033D1518, 0x031F0588)),
            "Hook ItemBag::uoe (Deserialize) -> auto-upgrade capacity",
        )
        # ItemBag..ctor (0x033D151C): mov w1, #bag_size; b #0x032D5890 (base ctor sets x3=xzr)
        mov_w1 = 0x52800001 | ((bag_size & 0xFFFF) << 5)
        patch_va(
            so,
            0x033D151C,
            bytes.fromhex("e3031faa"),  # mov x3, xzr
            struct.pack("<I", mov_w1),  # mov w1, #bag_size
            f"Patch ItemBag..ctor capacity -> {bag_size}",
        )

    return bytes(so)


def main():
    ap = argparse.ArgumentParser(description="Patch Shiren 6 Plus Android APK (3x Gold + 3x Backpack)")
    ap.add_argument("--gold-mult", type=int, default=3, help="Gold (Gitan) earn multiplier (1, 2, or 3; default: 3)")
    ap.add_argument("--bag-size", type=int, default=72, help="Backpack slots (default: 72 = 3x; 24 = vanilla)")
    args = ap.parse_args()
    assert 24 <= args.bag_size <= 200

    with open(os.path.join(DIR, "VERSION"), "r", encoding="utf-8") as f:
        version = f.read().strip()

    os.makedirs(OUT_DIR, exist_ok=True)
    print(f"Reading {IL2CPP_ENTRY} from {SRC_APK}...")
    with zipfile.ZipFile(SRC_APK, "r") as zin:
        so_bytes = zin.read(IL2CPP_ENTRY)

    patched_so = patch_libil2cpp(so_bytes, args.gold_mult, args.bag_size)

    unsigned_apk = os.path.join(OUT_DIR, f".tmp_shiren6_mod{version}.apk")
    signed_apk = os.path.join(OUT_DIR, f"Shiren6Plus_mod{version}.apk")

    print("Repacking 4KB-aligned Shiren 6 APK (1.4 GB — takes ~15-25s)...")
    apk_v2_sign.repack_apk_aligned(SRC_APK, unsigned_apk, {IL2CPP_ENTRY: patched_so})
    print("Signing with APK Signature Scheme v2...")
    apk_v2_sign.sign_apk_v2(unsigned_apk, signed_apk, KEY_DIR)
    os.remove(unsigned_apk)
    print(f"Done! Wrote: {signed_apk} ({os.path.getsize(signed_apk):,} bytes)")


if __name__ == "__main__":
    main()
