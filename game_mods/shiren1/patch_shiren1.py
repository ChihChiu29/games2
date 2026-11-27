#!/usr/bin/env python3
"""
Shiren the Wanderer DS (Shiren 1, NDS, AFUJ Chinese Fan Translation) Patcher
Applies:
  1. 3x Gold (Gitan) earn: hooks ARM9 AddGitan (0x02097774) via ARM helper at
     0x02135450 so positive Gitan gains (delta > 0) are multiplied by 3x while
     spending (delta < 0) is untouched.
  2. Power-of-2 64 MiB ROM padding + Header CRC16 recalculation for clean
     melonDS / flashcart compatibility.
"""

import argparse
import os
import struct


def crc16_nds(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc >> 1) ^ 0xA001) if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def encode_arm_bl(src_addr: int, dst_addr: int) -> bytes:
    rel = dst_addr - (src_addr + 8)
    if rel < -(1 << 25) or rel >= (1 << 25) or (rel & 3):
        raise ValueError(f"ARM BL out of range: {src_addr:#x} -> {dst_addr:#x}")
    imm24 = (rel >> 2) & 0xFFFFFF
    return struct.pack("<I", 0xEB000000 | imm24)


def ram_to_file(ram_addr: int, arm9_off: int, arm9_ram: int) -> int:
    return arm9_off + (ram_addr - arm9_ram)


def patch_bytes(buf: bytearray, off: int, expected: bytes, replacement: bytes, desc: str):
    actual = bytes(buf[off : off + len(expected)])
    if actual != expected:
        raise RuntimeError(
            f"{desc} at {off:#x}: expected {expected.hex()}, got {actual.hex()}"
        )
    buf[off : off + len(replacement)] = replacement
    print(f"  [OK] {desc} @ {off:#x}: {expected.hex()} -> {replacement.hex()}")


def build_arm_gitan_helper(mult: int) -> bytes:
    """
    ARM helper at 0x02135450 called from AddGitan (0x02097774).
    Input: r0 = signed delta, r2 = current Gitan
    Output: r0 = r2 + (r0 > 0 ? r0 * mult : r0)
    """
    if mult == 3:
        scale_insn = 0xC0800080  # addgt r0, r0, r0, lsl #1  (r0 * 3)
    elif mult == 2:
        scale_insn = 0xC1A00080  # movgt r0, r0, lsl #1      (r0 * 2)
    else:
        raise ValueError("Only --gold-mult 2 or 3 supported for Shiren 1 DS")
    return struct.pack(
        "<4I",
        0xE3500000,  # cmp   r0, #0
        scale_insn,  # scale positive delta
        0xE0820000,  # add   r0, r2, r0
        0xE12FFF1E,  # bx    lr
    )


def main():
    parser = argparse.ArgumentParser(description="Patch Shiren 1 DS (3x Gold + 64 MiB padding)")
    parser.add_argument("input_nds", help="Path to source shiren1.nds")
    parser.add_argument("output_nds", help="Path to output patched .nds")
    parser.add_argument("--gold-mult", type=int, default=3, help="Gold (Gitan) earn multiplier (2 or 3; default: 3)")
    args = parser.parse_args()

    with open(args.input_nds, "rb") as f:
        rom = bytearray(f.read())

    game_code = rom[0x0C:0x10].decode("ascii", errors="replace")
    arm9_off, arm9_entry, arm9_ram, arm9_size = struct.unpack_from("<IIII", rom, 0x20)
    print(f"Input ROM: {args.input_nds} ({len(rom)} bytes, GameCode={game_code})")
    if game_code != "AFUJ":
        raise RuntimeError(f"Unexpected game code {game_code!r}, expected 'AFUJ'")

    if args.gold_mult > 1:
        print(f"\n[1] Applying {args.gold_mult}x Gold (Gitan) earn patch...")
        helper_ram = 0x02135450
        helper = build_arm_gitan_helper(args.gold_mult)
        patch_bytes(
            rom,
            ram_to_file(helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(helper),
            helper,
            f"Install ARM AddGitan {args.gold_mult}x helper at {helper_ram:#x}",
        )
        patch_bytes(
            rom,
            ram_to_file(0x02097774, arm9_off, arm9_ram),
            b"\x00\x00\x82\xe0",  # add r0, r2, r0
            encode_arm_bl(0x02097774, helper_ram),
            "Hook AddGitan (0x02097774)",
        )

    # Pad 48 MiB ROM to 64 MiB (power of 2) with 0xFF and update Header CRC16
    target_size = 0x04000000
    if len(rom) < target_size:
        rom.extend(b"\xFF" * (target_size - len(rom)))
        print(f"\n[2] Padded ROM to {target_size:#x} (64 MiB)")

    hdr_crc = crc16_nds(bytes(rom[0x0000:0x015E]))
    struct.pack_into("<H", rom, 0x15E, hdr_crc)
    print(f"[3] Updated Header CRC16={hdr_crc:#06x}")

    os.makedirs(os.path.dirname(os.path.abspath(args.output_nds)), exist_ok=True)
    with open(args.output_nds, "wb") as f:
        f.write(rom)
    print(f"\nWrote patched ROM: {args.output_nds} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
