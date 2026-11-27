#!/usr/bin/env python3
"""
Shiren the Wanderer GB2: Desert Magic Castle (GBC, AFMJ) Patcher
Applies:
  1. 3x Gold (Gitan) earn: hooks Bank 0x07 AddCarriedGitan (0x5691, file 0x1D691)
     via an SM83 helper at Bank 0x07:0x7F00 (file 0x1FF00) that adds the 24-bit
     Gitan gain HRAM[0xFFB0..0xFFB2] to WRAM Bank 1 [0xD2A9..0xD2AC] 3 times
     before falling into the game's own 999,999 clamp.
  2. Recalculates the GBC Global Checksum (0x014E..0x014F).
"""

import argparse
import os
import struct


def patch_bytes(buf: bytearray, off: int, expected: bytes, replacement: bytes, desc: str):
    actual = bytes(buf[off : off + len(expected)])
    if actual != expected:
        raise RuntimeError(
            f"{desc} at {off:#x}: expected {expected.hex()}, got {actual.hex()}"
        )
    buf[off : off + len(replacement)] = replacement
    print(f"  [OK] {desc} @ {off:#x}: {expected.hex()} -> {replacement.hex()}")


def build_sm83_gitan_helper(mult: int) -> bytes:
    """
    SM83 helper at Bank 0x07:0x7F00 (file 0x1FF00) called from 0x5691.
    Runs (mult - 1) extra 32-bit additions of HRAM[0xFFB0..0xFFB2] into
    WRAM Bank 1 [0xD2A9..0xD2AC], reloads HL = 0xD2A9, and returns to 0x5694
    for the final addition + native 999,999 clamp.
    """
    extra = mult - 1
    assert 1 <= extra <= 9
    return bytes([
        0x06, extra,          # 7f00: ld b, extra
        0x21, 0xA9, 0xD2,     # 7f02: ld hl, 0xd2a9
        0xF0, 0xB0,           # 7f05: ldh a, (0xffb0)
        0x86,                 # 7f07: add a, (hl)
        0x22,                 # 7f08: ld (hl+), a
        0xF0, 0xB1,           # 7f09: ldh a, (0xffb1)
        0x8E,                 # 7f0b: adc a, (hl)
        0x22,                 # 7f0c: ld (hl+), a
        0xF0, 0xB2,           # 7f0d: ldh a, (0xffb2)
        0x8E,                 # 7f0f: adc a, (hl)
        0x22,                 # 7f10: ld (hl+), a
        0x3E, 0x00,           # 7f11: ld a, 0x00
        0x8E,                 # 7f13: adc a, (hl)
        0x77,                 # 7f14: ld (hl), a
        0x05,                 # 7f15: dec b
        0x20, 0xEA,           # 7f16: jr nz, 0x7f02 (-22)
        0x21, 0xA9, 0xD2,     # 7f18: ld hl, 0xd2a9
        0xC9,                 # 7f1b: ret
    ])


def main():
    parser = argparse.ArgumentParser(description="Patch Shiren GB2 (3x Gold earn)")
    parser.add_argument("input_gbc", help="Path to source shiren2.gbc")
    parser.add_argument("output_gbc", help="Path to output patched .gbc")
    parser.add_argument("--gold-mult", type=int, default=3, help="Gold (Gitan) earn multiplier (default: 3)")
    args = parser.parse_args()

    with open(args.input_gbc, "rb") as f:
        rom = bytearray(f.read())

    title = rom[0x134:0x143].decode("ascii", errors="replace").strip("\x00")
    print(f"Input ROM: {args.input_gbc} ({len(rom)} bytes, title={title!r})")

    if args.gold_mult > 1:
        print(f"\n[1] Applying {args.gold_mult}x Gold (Gitan) earn patch...")
        helper = build_sm83_gitan_helper(args.gold_mult)
        patch_bytes(
            rom,
            0x1FF00,  # Bank 0x07:0x7F00
            b"\x00" * len(helper),
            helper,
            f"Install SM83 AddCarriedGitan {args.gold_mult}x helper at Bank 0x07:0x7F00",
        )
        patch_bytes(
            rom,
            0x1D691,  # Bank 0x07:0x5691
            b"\x21\xa9\xd2",  # ld hl, 0xd2a9
            b"\xcd\x00\x7f",  # call 0x7f00
            "Hook Bank 0x07 AddCarriedGitan (0x5691)",
        )

    # Update GBC global checksum (0x014E..0x014F, big-endian)
    glob_chk = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    struct.pack_into(">H", rom, 0x14E, glob_chk)
    print(f"\n[2] Updated GBC Global Checksum={glob_chk:#06x}")

    os.makedirs(os.path.dirname(os.path.abspath(args.output_gbc)), exist_ok=True)
    with open(args.output_gbc, "wb") as f:
        f.write(rom)
    print(f"\nWrote patched ROM: {args.output_gbc} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
