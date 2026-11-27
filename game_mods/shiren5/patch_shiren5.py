#!/usr/bin/env python3
"""
Shiren the Wanderer 5 (NDS, B5FJ Chinese Fan Translation) Patcher
Applies:
  1. melonDS / real-hardware boot fix (EXMEMCNT Slot-1 ARM9 ownership wrapper
     for the fan translation's custom Chinese font loader + 64 MiB power-of-2
     ROM padding + Secure/Header CRC16 recalculation).
  2. Disable Night Mode (forces all dungeon floors to Always Day and prevents
     day->night transitions while allowing existing night saves to tick back
     to day naturally).
  3. 3x Gold (Gitan) earn (multiplies positive Gitan gains in dungeons and
     Gitan-bag usage by 3x; spending and bank withdrawals are untouched).
  4. Expanded Backpack (3x = 72 slots / 9 pages, or 2x = 48 slots / 6 pages;
     backwards-compatible with existing 24-slot saves).
"""

import argparse
import os
import struct


def crc16_nds(data: bytes) -> int:
    """Standard NDS MODBUS CRC-16 (poly 0xA001, init 0xFFFF)."""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def encode_thumb_bl(src_addr: int, dst_addr: int) -> bytes:
    """Encode a 32-bit Thumb-1 BL instruction from src_addr to dst_addr."""
    rel = dst_addr - (src_addr + 4)
    if rel < -(1 << 22) or rel >= (1 << 22) or (rel & 1):
        raise ValueError(f"BL out of range or unaligned: {src_addr:#x} -> {dst_addr:#x} (rel={rel:#x})")
    imm22 = (rel >> 1) & 0x3FFFFF
    h1 = 0xF000 | ((imm22 >> 11) & 0x7FF)
    h2 = 0xF800 | (imm22 & 0x7FF)
    return struct.pack("<HH", h1, h2)


def blz_decompress(data: bytes) -> bytearray:
    """Decompress an NDS BLZ (backward LZ77) overlay."""
    inc_size = struct.unpack_from("<I", data, len(data) - 4)[0]
    hdr_top = struct.unpack_from("<I", data, len(data) - 8)[0]
    comp_size = hdr_top & 0xFFFFFF
    bottom_size = hdr_top >> 24
    out_len = len(data) + inc_size
    out = bytearray(out_len)
    uncomp_prefix = len(data) - comp_size
    out[:uncomp_prefix] = data[:uncomp_prefix]
    src = len(data) - bottom_size
    dst = out_len
    while src > uncomp_prefix and dst > uncomp_prefix:
        src -= 1
        flags = data[src]
        for bit in range(7, -1, -1):
            if src <= uncomp_prefix or dst <= uncomp_prefix:
                break
            if flags & (1 << bit):
                src -= 2
                v = (data[src + 1] << 8) | data[src]
                length = ((v >> 12) & 0xF) + 3
                disp = (v & 0xFFF) + 3
                for _ in range(length):
                    dst -= 1
                    out[dst] = out[dst + disp]
            else:
                src -= 1
                dst -= 1
                out[dst] = data[src]
    return out


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


def build_exmemcnt_wrapper() -> bytes:
    """
    Thumb-1 wrapper at RAM 0x020D8834 (called via BLX to 0x020D8835) that
    temporarily clears bit 11 (0x0800) of REG_EXMEMCNT (0x04000204) so ARM9
    owns Slot-1 cartridge access before calling CARDi_ReadRomSyncCore
    (0x0209FF2D), then restores REG_EXMEMCNT.
    """
    return struct.pack(
        "<14HII",
        0xB570,  # push {r4, r5, r6, lr}
        0x4C06,  # ldr  r4, [pc, #0x18] -> 0x020D8850 (REG_EXMEMCNT)
        0x8825,  # ldrh r5, [r4]
        0x2608,  # movs r6, #8
        0x0236,  # lsls r6, r6, #8      -> 0x0800
        0x43F6,  # mvns r6, r6
        0x402E,  # ands r6, r5
        0x8026,  # strh r6, [r4]
        0x4E03,  # ldr  r6, [pc, #0x0c] -> 0x020D8854 (CARDi_ReadRomSyncCore)
        0x47B0,  # blx  r6
        0x8025,  # strh r5, [r4]
        0xBD70,  # pop  {r4, r5, r6, pc}
        0x46C0,  # nop
        0x46C0,  # nop
        0x04000204,  # REG_EXMEMCNT
        0x0209FF2D,  # CARDi_ReadRomSyncCore (Thumb)
    )


def build_dungeon_gitan_helper(mult: int) -> bytes:
    """
    Thumb-1 helper at RAM 0x020D8860 called from OV9[14] 0x0217814C
    (DungeonCharacter::AddGitan). Scales positive r1 by `mult` (2..7),
    then executes the replaced `movs r0, #0xeb; lsls r0, r0, #2`.
    """
    return struct.pack(
        "<7H",
        0x2900,           # cmp  r1, #0
        0xDD01,           # ble  +4 (skip 2 insns)
        0x2000 | mult,    # movs r0, #mult
        0x4341,           # muls r1, r0, r1
        0x20EB,           # movs r0, #0xeb
        0x0080,           # lsls r0, r0, #2
        0x4770,           # bx   lr
    )


def build_savedata_gitan_helper(mult: int) -> bytes:
    """
    Thumb-1 helper at RAM 0x020D8870 called from ARM9 0x02019F6C
    (SaveData::AddCarriedGitan). Scales positive r1 by `mult`, then
    executes the replaced `ldr r0, [r5, #0x34]; adds r1, r0, r1`.
    """
    return struct.pack(
        "<7H",
        0x2900,           # cmp  r1, #0
        0xDD01,           # ble  +4 (skip 2 insns)
        0x2000 | mult,    # movs r0, #mult
        0x4341,           # muls r1, r0, r1
        0x6B68,           # ldr  r0, [r5, #0x34]
        0x1841,           # adds r1, r0, r1
        0x4770,           # bx   lr
    )


def build_alloc_itembag_helper(helper_ram: int, bag_size: int) -> bytes:
    """
    Thumb-1 helper at RAM 0x020D8880 replacing `new ItemBag` (size 0x7C).
    Allocates `0x7C + bag_size * 4` bytes so `this + 0x78` stays intact and
    the expanded `Item*` pointer array lives at `this + 0x7C`.
    """
    alloc_bytes = 0x7C + bag_size * 4
    assert alloc_bytes % 4 == 0 and (alloc_bytes >> 2) <= 0xFF
    imm = alloc_bytes >> 2
    code = bytearray()
    code += struct.pack("<HHH", 0xB500, 0x2000 | imm, 0x0080)  # push {lr}; movs r0, #imm; lsls r0, r0, #2
    code += encode_thumb_bl(helper_ram + 6, 0x020096F0)        # bl operator_new
    code += struct.pack("<H", 0xBD00)                          # pop {pc}
    return bytes(code)


def build_itembag_deser_helper(helper_ram: int) -> bytes:
    """
    Thumb-1 helper at RAM 0x020D8890 called from ItemBag::Deserialize
    (0x02049450). Calls ItemContainer::Deserialize then sets
    `current_capacity = max_capacity` so existing 24-slot saves are
    automatically upgraded to the expanded capacity.
    """
    code = bytearray()
    code += struct.pack("<H", 0xB500)                          # push {lr}
    code += encode_thumb_bl(helper_ram + 2, 0x02047F80)        # bl ItemContainer::Deserialize
    code += struct.pack("<HHH", 0x68B8, 0x60F8, 0xBD00)        # ldr r0, [r7, #8]; str r0, [r7, #0xc]; pop {pc}
    return bytes(code)


def build_itembag_bracelet_check_helper() -> bytes:
    """
    Thumb-1 helper at RAM 0x020D88A0 called from ItemBag::Deserialize
    (0x020494D0). Compares saved bracelet slot index (r1) against
    item_count ([r7, #0x10]) so both old (0x18) and new (bag_size) "none"
    sentinels safely skip equipping without out-of-bounds lookup.
    """
    return struct.pack(
        "<4H",
        0x5981,  # ldr r1, [r0, r6]
        0x6938,  # ldr r0, [r7, #0x10]
        0x4281,  # cmp r1, r0
        0x4770,  # bx  lr
    )


def build_waitframes_skip_helper() -> bytes:
    """
    Thumb-1 helper at RAM 0x020D88B0 called from WaitFrames (0x02009A5A).
    Replaces `adds r5, r0, #0; movs r4, #0`. If not in a dungeon scene
    (scene_id != 2 and scene_id != 3) and L or R is held in REG_KEYINPUT
    (0x04000130, active-low bits 8/9), clamps wait duration r5 to 1 frame
    so opening logos, cutscene animations, and scripted pauses skip cleanly.
    """
    return struct.pack(
        "<24HII",
        0x1C05,  # +00: adds r5, r0, #0
        0x2400,  # +02: movs r4, #0
        0x2D01,  # +04: cmp  r5, #1
        0xDD11,  # +06: ble  +0x2c (bx lr)
        0x4809,  # +08: ldr  r0, [pc, #0x24] -> +0x30 (0x0210E68C g_SceneManager*)
        0x6800,  # +0a: ldr  r0, [r0]
        0x2800,  # +0c: cmp  r0, #0
        0xD005,  # +0e: beq  +0x1c (early boot/logo -> allow skip!)
        0x3088,  # +10: adds r0, #0x88
        0x7800,  # +12: ldrb r0, [r0]
        0x2802,  # +14: cmp  r0, #2          (dungeon scene 2)
        0xD009,  # +16: beq  +0x2c (bx lr)
        0x2803,  # +18: cmp  r0, #3          (dungeon scene 3)
        0xD007,  # +1a: beq  +0x2c (bx lr)
        0x4805,  # +1c: ldr  r0, [pc, #0x14] -> +0x34 (0x04000130 REG_KEYINPUT)
        0x8801,  # +1e: ldrh r1, [r0]
        0x43C9,  # +20: mvns r1, r1          (active-low -> active-high)
        0x2003,  # +22: movs r0, #3
        0x0200,  # +24: lsls r0, r0, #8      (0x0300 = R | L)
        0x4201,  # +26: tst  r1, r0
        0xD000,  # +28: beq  +0x2c (bx lr)
        0x2501,  # +2a: movs r5, #1
        0x4770,  # +2c: bx   lr
        0x46C0,  # +2e: nop  (4-byte align)
        0x0210E68C,  # +30: g_SceneManager*
        0x04000130,  # +34: REG_KEYINPUT
    )


def main():
    parser = argparse.ArgumentParser(
        description="Patch Shiren 5 NDS (melonDS boot fix + No Night + 3x Gold + Expanded Backpack)"
    )
    parser.add_argument("input_nds", help="Path to source shiren5.nds")
    parser.add_argument("output_nds", help="Path to output patched .nds")
    parser.add_argument("--keep-night", action="store_true", help="Keep Night Mode enabled")
    parser.add_argument("--gold-mult", type=int, default=3, help="Gold (Gitan) earn multiplier (default: 3; 1=vanilla)")
    parser.add_argument("--bag-size", type=int, default=72, help="Backpack slot count (default: 72 = 3x; 48 = 2x; 24 = vanilla)")
    parser.add_argument("--no-lr-skip", action="store_true", help="Disable L/R dialogue fast-forward skip")
    args = parser.parse_args()

    if not (1 <= args.gold_mult <= 10):
        raise ValueError("--gold-mult must be between 1 and 10")
    if not (24 <= args.bag_size <= 80):
        raise ValueError("--bag-size must be between 24 and 80")

    with open(args.input_nds, "rb") as f:
        rom = bytearray(f.read())

    orig_len = len(rom)
    game_code = rom[0x0C:0x10].decode("ascii", errors="replace")
    arm9_off, arm9_entry, arm9_ram, arm9_size = struct.unpack_from("<IIII", rom, 0x20)
    ov9_off, ov9_size = struct.unpack_from("<II", rom, 0x50)
    fat_off, fat_size = struct.unpack_from("<II", rom, 0x48)

    print(f"Input ROM: {args.input_nds} ({orig_len} bytes, GameCode={game_code})")
    print(f"  ARM9: file={arm9_off:#x}, ram={arm9_ram:#x}, entry={arm9_entry:#x}, size={arm9_size:#x}")
    if game_code != "B5FJ":
        raise RuntimeError(f"Unexpected game code {game_code!r}, expected 'B5FJ'")

    # 1. melonDS boot fix: EXMEMCNT wrapper for fan-translation font loader
    print("\n[1] Applying melonDS boot fix (EXMEMCNT Slot-1 wrapper + padding + CRCs)...")
    wrapper = build_exmemcnt_wrapper()
    wrapper_ram = 0x020D8834
    patch_bytes(
        rom,
        ram_to_file(0x020D8830, arm9_off, arm9_ram),
        struct.pack("<I", 0x0209FF2D),
        struct.pack("<I", wrapper_ram | 1),
        "Redirect font-loader CARDi_ReadRomSyncCore ptr -> EXMEMCNT wrapper",
    )
    patch_bytes(
        rom,
        ram_to_file(wrapper_ram, arm9_off, arm9_ram),
        b"\x00" * len(wrapper),
        wrapper,
        "Install EXMEMCNT wrapper at 0x020D8834",
    )

    # 2. Disable Night Mode
    if not args.keep_night:
        print("\n[2] Applying No-Night-Mode patch...")
        patch_bytes(
            rom,
            ram_to_file(0x02026084, arm9_off, arm9_ram),
            b"\x00\x78",  # ldrb r0, [r0]
            b"\x00\x20",  # movs r0, #0
            "InitFloorDayNight: force Always Day (mode 0)",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0202E32C, arm9_off, arm9_ram),
            b"\x29\x5c",  # ldrb r1, [r5, r0]
            b"\x00\x21",  # movs r1, #0
            "UpdateDayNight: force Always Day (mode 0)",
        )

    # 3. Gold (Gitan) earn multiplier
    if args.gold_mult > 1:
        print(f"\n[3] Applying {args.gold_mult}x Gold (Gitan) earn patch...")
        # 3a. Install helpers in ARM9 free cave
        ov14_helper_ram = 0x020D8860
        ov14_helper = build_dungeon_gitan_helper(args.gold_mult)
        patch_bytes(
            rom,
            ram_to_file(ov14_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(ov14_helper),
            ov14_helper,
            f"Install DungeonAddGitan {args.gold_mult}x helper at {ov14_helper_ram:#x}",
        )

        sd_helper_ram = 0x020D8870
        sd_helper = build_savedata_gitan_helper(args.gold_mult)
        patch_bytes(
            rom,
            ram_to_file(sd_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(sd_helper),
            sd_helper,
            f"Install SaveDataAddGitan {args.gold_mult}x helper at {sd_helper_ram:#x}",
        )

        # Hook SaveData::AddCarriedGitan (ARM9 0x02019F6C)
        patch_bytes(
            rom,
            ram_to_file(0x02019F6C, arm9_off, arm9_ram),
            b"\x68\x6b\x41\x18",  # ldr r0, [r5, #0x34]; adds r1, r0, r1
            encode_thumb_bl(0x02019F6C, sd_helper_ram),
            "Hook SaveData::AddCarriedGitan (0x02019F6C)",
        )

        # 3b. Decompress OV9[14], hook DungeonCharacter::AddGitan (0x0217814C), store uncompressed
        ov14_ent_off = ov9_off + 14 * 32
        ov_id, ov_ram, ov_ram_size, ov_bss, ov_si_s, ov_si_e, ov_file_id, ov_flags = struct.unpack_from(
            "<IIIIIIII", rom, ov14_ent_off
        )
        f_start, f_end = struct.unpack_from("<II", rom, fat_off + ov_file_id * 8)
        ov14_raw = bytes(rom[f_start:f_end])
        ov14 = blz_decompress(ov14_raw) if (ov_flags & 0x01000000) else bytearray(ov14_raw)
        assert len(ov14) == ov_ram_size, f"OV9[14] decompressed len {len(ov14)} != {ov_ram_size}"

        ov14_hook_off = 0x0217814C - ov_ram
        patch_bytes(
            ov14,
            ov14_hook_off,
            b"\xeb\x20\x80\x00",  # movs r0, #0xeb; lsls r0, r0, #2
            encode_thumb_bl(0x0217814C, ov14_helper_ram),
            "Hook OV9[14] DungeonCharacter::AddGitan (0x0217814C)",
        )

        # Append uncompressed OV9[14] aligned to 0x200 at end of used ROM
        new_ov_start = (len(rom) + 0x1FF) & ~0x1FF
        if new_ov_start > len(rom):
            rom.extend(b"\xFF" * (new_ov_start - len(rom)))
        rom.extend(ov14)
        new_ov_end = len(rom)
        struct.pack_into("<II", rom, fat_off + ov_file_id * 8, new_ov_start, new_ov_end)
        # Clear compressed flag (bit 24) and set compressed_size field (bits 0..23) to 0
        new_flags = ov_flags & ~0x01FFFFFF
        struct.pack_into("<I", rom, ov14_ent_off + 28, new_flags)
        print(
            f"  [OK] Relocated uncompressed OV9[14] to file {new_ov_start:#x}..{new_ov_end:#x} "
            f"({len(ov14)} B), flags {ov_flags:#x} -> {new_flags:#x}"
        )

    # 4. Expanded Backpack (ItemBag)
    if args.bag_size > 24:
        print(f"\n[4] Expanding Backpack (ItemBag) from 24 -> {args.bag_size} slots...")
        alloc_helper_ram = 0x020D8880
        alloc_helper = build_alloc_itembag_helper(alloc_helper_ram, args.bag_size)
        patch_bytes(
            rom,
            ram_to_file(alloc_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(alloc_helper),
            alloc_helper,
            f"Install AllocItemBag helper (0x{0x7C + args.bag_size * 4:x} B) at {alloc_helper_ram:#x}",
        )

        deser_helper_ram = 0x020D8890
        deser_helper = build_itembag_deser_helper(deser_helper_ram)
        patch_bytes(
            rom,
            ram_to_file(deser_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(deser_helper),
            deser_helper,
            f"Install ItemBagDeserialize capacity-upgrade helper at {deser_helper_ram:#x}",
        )

        brac_helper_ram = 0x020D88A0
        brac_helper = build_itembag_bracelet_check_helper()
        patch_bytes(
            rom,
            ram_to_file(brac_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(brac_helper),
            brac_helper,
            f"Install ItemBag bracelet bounds-check helper at {brac_helper_ram:#x}",
        )

        # Patch both `new ItemBag` call sites (0x0203C3E0 and 0x0203C584)
        patch_bytes(
            rom,
            ram_to_file(0x0203C3E0, arm9_off, arm9_ram),
            b"\x7c\x20\xcd\xf7\x85\xf9",  # movs r0, #0x7c; bl 0x20096f0
            encode_thumb_bl(0x0203C3E0, alloc_helper_ram) + b"\xc0\x46",  # bl helper; nop
            "ItemBag alloc #1 (main bag) -> expanded size",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0203C584, arm9_off, arm9_ram),
            b"\x38\x1c\xcd\xf7\xb3\xf8",  # adds r0, r7, #0; bl 0x20096f0
            encode_thumb_bl(0x0203C584, alloc_helper_ram) + b"\xc0\x46",  # bl helper; nop
            "ItemBag alloc #2 (secondary bags) -> expanded size",
        )

        # Patch ItemBag::ItemBag (0x0204812A): ptr_array = this + 0x7C, capacity = bag_size
        patch_bytes(
            rom,
            ram_to_file(0x0204812A, arm9_off, arm9_ram),
            b"\x18\x31\x18\x22",  # adds r1, #0x18; movs r2, #0x18
            struct.pack("<HH", 0x317C, 0x2200 | args.bag_size),
            f"ItemBag::ItemBag: ptr_array=this+0x7C, capacity={args.bag_size}",
        )

        # Patch ItemBag::Deserialize (0x02049450) to upgrade existing 24-slot saves
        patch_bytes(
            rom,
            ram_to_file(0x02049450, arm9_off, arm9_ram),
            b"\xfe\xf7\x96\xfd",  # bl 0x2047f80
            encode_thumb_bl(0x02049450, deser_helper_ram),
            "ItemBag::Deserialize: auto-upgrade loaded capacity to max_capacity",
        )

        # Patch ItemBag::Deserialize bracelet sentinel check (0x020494D0)
        patch_bytes(
            rom,
            ram_to_file(0x020494D0, arm9_off, arm9_ram),
            b"\x81\x59\x18\x29\x0e\xd0",  # ldr r1, [r0, r6]; cmp r1, #0x18; beq +0x20
            encode_thumb_bl(0x020494D0, brac_helper_ram) + b"\x0e\xd2",  # bl helper; bhs +0x20
            "ItemBag::Deserialize: bounds-checked bracelet index (old & new saves)",
        )

        # Patch ItemBag::Serialize "no bracelet" sentinel (0x18 -> 0xFF)
        patch_bytes(
            rom,
            ram_to_file(0x0204957A, arm9_off, arm9_ram),
            b"\x18\x20",  # movs r0, #0x18
            b"\xff\x20",  # movs r0, #0xff
            "ItemBag::Serialize: no-bracelet sentinel #1 -> 0xFF",
        )
        patch_bytes(
            rom,
            ram_to_file(0x02049580, arm9_off, arm9_ram),
            b"\x18\x21",  # movs r1, #0x18
            b"\xff\x21",  # movs r1, #0xff
            "ItemBag::Serialize: no-bracelet sentinel #2 -> 0xFF",
        )

    # 5. L/R/X Dialogue Skip (fast-forward any conversation while holding L, R, or X)
    if not args.no_lr_skip:
        print("\n[5] Applying L/R/X conversation skip patch...")
        patch_bytes(
            rom,
            ram_to_file(0x0201E806, arm9_off, arm9_ram),
            b"\x28\x78",  # ldrb r0, [r5]
            b"\x0c\xe0",  # b #0x201e822 (bypass scene/enable restrictions)
            "IsDialogueSkipHeld: enable in all conversations & scenes",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0201E84C, arm9_off, arm9_ram),
            b"\x01\x20\x80\x02",  # movs r0, #1; lsls r0, r0, #0xa (0x0400 = X)
            b"\x07\x20\x00\x02",  # movs r0, #7; lsls r0, r0, #8   (0x0700 = R|L|X)
            "IsDialogueSkipHeld: accept L, R, or X button",
        )
        # Reduce the 30-frame (0.5s) pause between skipped dialogue pages/boxes -> 1 frame
        for idx, addr in enumerate((0x02067C4E, 0x02067DF4, 0x0206805E, 0x0206846E), 1):
            patch_bytes(
                rom,
                ram_to_file(addr, arm9_off, arm9_ram),
                b"\x1e\x20",  # movs r0, #0x1e (30 frames)
                b"\x01\x20",  # movs r0, #1    (1 frame)
                f"Dialogue skip pause #{idx} ({addr:#x}): 30 frames -> 1 frame",
            )
        # Hook WaitFrames (0x02009A5A) to fast-forward opening/cutscene waits when L or R is held outside dungeons
        wf_helper_ram = 0x020D88B0
        wf_helper = build_waitframes_skip_helper()
        patch_bytes(
            rom,
            ram_to_file(wf_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(wf_helper),
            wf_helper,
            f"Install WaitFrames L/R cutscene-skip helper at {wf_helper_ram:#x}",
        )
        patch_bytes(
            rom,
            ram_to_file(0x02009A5A, arm9_off, arm9_ram),
            b"\x05\x1c\x00\x24",  # adds r5, r0, #0; movs r4, #0
            encode_thumb_bl(0x02009A5A, wf_helper_ram),
            "Hook WaitFrames (0x02009A5A) for L/R opening/cutscene skip",
        )

    # 6. Update total used ROM size header field (0x80) and pad to 64 MiB (0x04000000)
    used_size = len(rom)
    target_size = 0x04000000
    if used_size > target_size:
        raise RuntimeError(f"Patched ROM ({used_size:#x}) exceeds 64 MiB!")
    struct.pack_into("<I", rom, 0x80, used_size)
    pad_len = target_size - used_size
    if pad_len > 0:
        rom.extend(b"\xFF" * pad_len)
        print(f"\n[5] Updated header rom_size={used_size:#x} and padded to {target_size:#x} (64 MiB)")

    # 6. Recalculate Secure Area CRC16 (0x06C) and Header CRC16 (0x15E)
    sec_crc = crc16_nds(bytes(rom[0x4000:0x8000]))
    struct.pack_into("<H", rom, 0x06C, sec_crc)
    hdr_crc = crc16_nds(bytes(rom[0x0000:0x015E]))
    struct.pack_into("<H", rom, 0x15E, hdr_crc)
    print(f"[6] Updated Secure CRC16={sec_crc:#06x}, Header CRC16={hdr_crc:#06x}")

    os.makedirs(os.path.dirname(os.path.abspath(args.output_nds)), exist_ok=True)
    with open(args.output_nds, "wb") as f:
        f.write(rom)
    print(f"\nWrote patched ROM: {args.output_nds} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
