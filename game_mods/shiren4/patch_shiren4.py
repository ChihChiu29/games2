#!/usr/bin/env python3
"""
Shiren the Wanderer 4 (NDS, B4FJ Chinese Fan Translation) Patcher
Applies:
  1. melonDS / real-hardware boot fix (EXMEMCNT Slot-1 ARM9 ownership wrapper
     around the fan translation's ARM ROM reader at 0x020DFB50 + 64 MiB
     power-of-2 ROM padding + Secure/Header CRC16 recalculation).
  2. Disable Night Mode (forces all dungeon floors to Always Day and prevents
     day->night transitions while allowing existing night saves to tick back
     to day naturally).
  3. 3x Gold (Gitan) earn (multiplies positive Gitan gains in dungeons and
     Gitan-bag usage by 3x; spending and bank withdrawals are untouched).
  4. 3x Backpack Size (72 slots / 9 pages, up from 24; auto-upgrades existing
     24-slot saves safely).
  5. Hold L / R / X to skip conversations (1-frame delay between text boxes)
     and hold L / R outside dungeons to skip opening/cutscene animation waits.
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


def encode_thumb_bl(src_addr: int, dst_addr: int) -> bytes:
    rel = dst_addr - (src_addr + 4)
    if rel < -(1 << 22) or rel >= (1 << 22) or (rel & 1):
        raise ValueError(f"Thumb BL out of range: {src_addr:#x} -> {dst_addr:#x}")
    imm22 = (rel >> 1) & 0x3FFFFF
    h1 = 0xF000 | ((imm22 >> 11) & 0x7FF)
    h2 = 0xF800 | (imm22 & 0x7FF)
    return struct.pack("<HH", h1, h2)


def encode_arm_bl(src_addr: int, dst_addr: int) -> bytes:
    rel = dst_addr - (src_addr + 8)
    if rel < -(1 << 25) or rel >= (1 << 25) or (rel & 3):
        raise ValueError(f"ARM BL out of range: {src_addr:#x} -> {dst_addr:#x}")
    imm24 = (rel >> 2) & 0xFFFFFF
    return struct.pack("<I", 0xEB000000 | imm24)


def blz_decompress(data: bytes) -> bytearray:
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


def build_arm_exmemcnt_wrapper(wrapper_ram: int) -> bytes:
    """
    ARM wrapper at 0x020CA080 around 0x020DFB50 (fan-translation ROM reader).
    Clears bit 11 (0x0800) of REG_EXMEMCNT (0x04000204) before calling
    0x020DFB50 and restores it afterwards.
    """
    code = bytearray()
    code += struct.pack(
        "<5I",
        0xE92D4070,  # +00: push {r4, r5, r6, lr}
        0xE59F4014,  # +04: ldr  r4, [pc, #0x14] -> +0x20 (0x04000204 REG_EXMEMCNT)
        0xE1D450B0,  # +08: ldrh r5, [r4]
        0xE3C56B02,  # +0c: bic  r6, r5, #0x800
        0xE1C460B0,  # +10: strh r6, [r4]
    )
    code += encode_arm_bl(wrapper_ram + 0x14, 0x020DFB50)  # +14: bl 0x020DFB50
    code += struct.pack(
        "<3I",
        0xE1C450B0,  # +18: strh r5, [r4]
        0xE8BD8070,  # +1c: pop  {r4, r5, r6, pc}
        0x04000204,  # +20: REG_EXMEMCNT
    )
    return bytes(code)


def build_dungeon_gitan_helper(mult: int) -> bytes:
    """
    Thumb-1 helper at 0x020CA0B0 called from OV9[2] 0x02143AA4
    (DungeonCharacter::AddGitan). Scales positive r1 by `mult`, then
    executes replaced `movs r0, #0xdb; lsls r0, r0, #2`.
    """
    return struct.pack(
        "<7H",
        0x2900,           # cmp  r1, #0
        0xDD01,           # ble  +4
        0x2000 | mult,    # movs r0, #mult
        0x4341,           # muls r1, r0, r1
        0x20DB,           # movs r0, #0xdb
        0x0080,           # lsls r0, r0, #2
        0x4770,           # bx   lr
    )


def build_savedata_gitan_helper(mult: int) -> bytes:
    """
    Thumb-1 helper at 0x020CA0C0 called from ARM9 0x020186EA
    (SaveData::AddCarriedGitan). Scales positive r1 by `mult`, then
    executes replaced `ldr r0, [r4, #0x28]; adds r1, r0, r1`.
    """
    return struct.pack(
        "<7H",
        0x2900,           # cmp  r1, #0
        0xDD01,           # ble  +4
        0x2000 | mult,    # movs r0, #mult
        0x4341,           # muls r1, r0, r1
        0x6AA0,           # ldr  r0, [r4, #0x28]
        0x1841,           # adds r1, r0, r1
        0x4770,           # bx   lr
    )


def build_itembag_deser_helper(helper_ram: int) -> bytes:
    """
    Thumb-1 helper at 0x020CA0D0 called from ItemBag::Deserialize (0x0203D76A).
    Calls ItemContainer::Deserialize (0x0203CBFC), then sets
    `current_capacity = max_capacity` so existing 24-slot saves upgrade.
    """
    code = bytearray()
    code += struct.pack("<H", 0xB500)                        # push {lr}
    code += encode_thumb_bl(helper_ram + 2, 0x0203CBFC)      # bl ItemContainer::Deserialize
    code += struct.pack("<HHH", 0x68B8, 0x60F8, 0xBD00)      # ldr r0, [r7, #8]; str r0, [r7, #0xc]; pop {pc}
    return bytes(code)


def build_itembag_bracelet_check_helper() -> bytes:
    """
    Thumb-1 helper at 0x020CA0E0 called from ItemBag::Deserialize (0x0203D84C).
    Loads saved bracelet index r1 = [r1, r0] and compares against item_count
    ([r7, #0x10]) so both 0x18 (old saves) and 0xFF (new saves) safely skip.
    """
    return struct.pack(
        "<4H",
        0x5809,  # ldr r1, [r1, r0]
        0x693A,  # ldr r2, [r7, #0x10]
        0x4291,  # cmp r1, r2
        0x4770,  # bx  lr
    )


def build_waitframes_skip_helper() -> bytes:
    """
    Thumb-1 helper at 0x020CA0F0 called from WaitFrames (0x02008F62).
    Replaces `adds r5, r0, #0; movs r4, #0`. Outside dungeons
    (scene_id at [g_SceneManager + 0x84] != 2 and != 3), if L or R is held
    in REG_KEYINPUT (0x04000130), clamps wait duration r5 to 1 frame.
    """
    return struct.pack(
        "<24HII",
        0x1C05,  # +00: adds r5, r0, #0
        0x2400,  # +02: movs r4, #0
        0x2D01,  # +04: cmp  r5, #1
        0xDD11,  # +06: ble  +0x2c (bx lr)
        0x4809,  # +08: ldr  r0, [pc, #0x24] -> +0x30 (0x020F0AC4 g_SceneManager*)
        0x6800,  # +0a: ldr  r0, [r0]
        0x2800,  # +0c: cmp  r0, #0
        0xD005,  # +0e: beq  +0x1c (early boot/logo -> allow skip!)
        0x3084,  # +10: adds r0, #0x84       (Shiren 4 scene_id offset!)
        0x7800,  # +12: ldrb r0, [r0]
        0x2802,  # +14: cmp  r0, #2
        0xD009,  # +16: beq  +0x2c (bx lr)
        0x2803,  # +18: cmp  r0, #3
        0xD007,  # +1a: beq  +0x2c (bx lr)
        0x4805,  # +1c: ldr  r0, [pc, #0x14] -> +0x34 (0x04000130 REG_KEYINPUT)
        0x8801,  # +1e: ldrh r1, [r0]
        0x43C9,  # +20: mvns r1, r1
        0x2003,  # +22: movs r0, #3
        0x0200,  # +24: lsls r0, r0, #8      (0x0300 = R | L)
        0x4201,  # +26: tst  r1, r0
        0xD000,  # +28: beq  +0x2c (bx lr)
        0x2501,  # +2a: movs r5, #1
        0x4770,  # +2c: bx   lr
        0x46C0,  # +2e: nop
        0x020F0AC4,  # +30: g_SceneManager*
        0x04000130,  # +34: REG_KEYINPUT
    )


def main():
    parser = argparse.ArgumentParser(
        description="Patch Shiren 4 NDS (melonDS fix + No Night + 3x Gold + 3x Backpack + L/R Skip)"
    )
    parser.add_argument("input_nds", help="Path to source shiren4.nds")
    parser.add_argument("output_nds", help="Path to output patched .nds")
    parser.add_argument("--keep-night", action="store_true", help="Keep Night Mode enabled")
    parser.add_argument("--gold-mult", type=int, default=3, help="Gold earn multiplier (default: 3)")
    parser.add_argument("--bag-size", type=int, default=72, help="Backpack slots (default: 72 = 3x; 48 = 2x; 24 = vanilla)")
    parser.add_argument("--no-lr-skip", action="store_true", help="Disable L/R dialogue & cutscene skip")
    args = parser.parse_args()

    if not (1 <= args.gold_mult <= 10):
        raise ValueError("--gold-mult must be 1..10")
    if not (24 <= args.bag_size <= 80) or (args.bag_size % 4 != 0):
        raise ValueError("--bag-size must be a multiple of 4 between 24 and 80")

    with open(args.input_nds, "rb") as f:
        rom = bytearray(f.read())

    game_code = rom[0x0C:0x10].decode("ascii", errors="replace")
    arm9_off, arm9_entry, arm9_ram, arm9_size = struct.unpack_from("<IIII", rom, 0x20)
    ov9_off, ov9_size = struct.unpack_from("<II", rom, 0x50)
    fat_off, fat_size = struct.unpack_from("<II", rom, 0x48)

    print(f"Input ROM: {args.input_nds} ({len(rom)} bytes, GameCode={game_code})")
    if game_code != "B4FJ":
        raise RuntimeError(f"Unexpected game code {game_code!r}, expected 'B4FJ'")

    # 1. melonDS / real-hardware EXMEMCNT boot fix
    print("\n[1] Applying melonDS boot fix (ARM EXMEMCNT wrapper + padding + CRCs)...")
    ex_ram = 0x020CA080
    ex_code = build_arm_exmemcnt_wrapper(ex_ram)
    patch_bytes(
        rom,
        ram_to_file(ex_ram, arm9_off, arm9_ram),
        b"\x00" * len(ex_code),
        ex_code,
        f"Install ARM EXMEMCNT wrapper at {ex_ram:#x}",
    )
    for caller in (0x020DFAC8, 0x020DFB30, 0x020DFB40):
        patch_bytes(
            rom,
            ram_to_file(caller, arm9_off, arm9_ram),
            encode_arm_bl(caller, 0x020DFB50),
            encode_arm_bl(caller, ex_ram),
            f"Redirect ARM ROM reader call at {caller:#x} -> EXMEMCNT wrapper",
        )

    # 2. Disable Night Mode
    if not args.keep_night:
        print("\n[2] Applying No-Night-Mode patch...")
        patch_bytes(
            rom,
            ram_to_file(0x0202255E, arm9_off, arm9_ram),
            b"\x00\x78",  # ldrb r0, [r0]
            b"\x00\x20",  # movs r0, #0
            "InitFloorDayNight: force Always Day (mode 0)",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0202844C, arm9_off, arm9_ram),
            b"\x01\x78",  # ldrb r1, [r0]
            b"\x00\x21",  # movs r1, #0
            "UpdateDayNight: force Always Day (mode 0)",
        )

    # 3. 3x Gold (Gitan) earn
    if args.gold_mult > 1:
        print(f"\n[3] Applying {args.gold_mult}x Gold (Gitan) earn patch...")
        ov2_helper_ram = 0x020CA0B0
        ov2_helper = build_dungeon_gitan_helper(args.gold_mult)
        patch_bytes(
            rom,
            ram_to_file(ov2_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(ov2_helper),
            ov2_helper,
            f"Install DungeonAddGitan {args.gold_mult}x helper at {ov2_helper_ram:#x}",
        )
        sd_helper_ram = 0x020CA0C0
        sd_helper = build_savedata_gitan_helper(args.gold_mult)
        patch_bytes(
            rom,
            ram_to_file(sd_helper_ram, arm9_off, arm9_ram),
            b"\x00" * len(sd_helper),
            sd_helper,
            f"Install SaveDataAddGitan {args.gold_mult}x helper at {sd_helper_ram:#x}",
        )
        patch_bytes(
            rom,
            ram_to_file(0x020186EA, arm9_off, arm9_ram),
            b"\xa0\x6a\x41\x18",  # ldr r0, [r4, #0x28]; adds r1, r0, r1
            encode_thumb_bl(0x020186EA, sd_helper_ram),
            "Hook SaveData::AddCarriedGitan (0x020186EA)",
        )

        # Decompress OV9[2], hook DungeonCharacter::AddGitan (0x02143AA4), relocate uncompressed
        ov2_ent_off = ov9_off + 2 * 32
        ov_id, ov_ram, ov_ram_size, ov_bss, ov_si_s, ov_si_e, ov_file_id, ov_flags = struct.unpack_from(
            "<IIIIIIII", rom, ov2_ent_off
        )
        f_start, f_end = struct.unpack_from("<II", rom, fat_off + ov_file_id * 8)
        ov2_raw = bytes(rom[f_start:f_end])
        ov2 = blz_decompress(ov2_raw) if (ov_flags & 0x01000000) else bytearray(ov2_raw)
        assert len(ov2) == ov_ram_size

        patch_bytes(
            ov2,
            0x02143AA4 - ov_ram,
            b"\xdb\x20\x80\x00",  # movs r0, #0xdb; lsls r0, r0, #2
            encode_thumb_bl(0x02143AA4, ov2_helper_ram),
            "Hook OV9[2] DungeonCharacter::AddGitan (0x02143AA4)",
        )
        new_ov_start = (len(rom) + 0x1FF) & ~0x1FF
        if new_ov_start > len(rom):
            rom.extend(b"\xFF" * (new_ov_start - len(rom)))
        rom.extend(ov2)
        new_ov_end = len(rom)
        struct.pack_into("<II", rom, fat_off + ov_file_id * 8, new_ov_start, new_ov_end)
        new_flags = ov_flags & ~0x01FFFFFF
        struct.pack_into("<I", rom, ov2_ent_off + 28, new_flags)
        print(f"  [OK] Relocated uncompressed OV9[2] to {new_ov_start:#x}..{new_ov_end:#x} ({len(ov2)} B)")

    # 4. Expanded Backpack (ItemBag)
    if args.bag_size > 24:
        print(f"\n[4] Expanding Backpack (ItemBag) from 24 -> {args.bag_size} slots...")
        alloc_bytes = 0x18 + args.bag_size * 4
        # Encode ARM `mov r0, #alloc_bytes` (imm8 rotated right by 30 = << 2)
        assert alloc_bytes % 4 == 0 and (alloc_bytes >> 2) <= 0xFF
        arm_mov_r0 = 0xE3A00F00 | (alloc_bytes >> 2)
        patch_bytes(
            rom,
            ram_to_file(0x0203481C, arm9_off, arm9_ram),
            struct.pack("<I", 0xE1A00005),  # mov r0, r5 (0x78)
            struct.pack("<I", arm_mov_r0),  # mov r0, #alloc_bytes
            f"ItemBag alloc (0x0203481C): 0x78 -> {alloc_bytes:#x} B",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0203CD94, arm9_off, arm9_ram),
            b"\x18\x22",  # movs r2, #0x18
            struct.pack("<H", 0x2200 | args.bag_size),
            f"ItemBag::ItemBag (0x0203CD94): capacity -> {args.bag_size}",
        )

        deser_ram = 0x020CA0D0
        deser_code = build_itembag_deser_helper(deser_ram)
        patch_bytes(
            rom,
            ram_to_file(deser_ram, arm9_off, arm9_ram),
            b"\x00" * len(deser_code),
            deser_code,
            f"Install ItemBagDeserialize upgrade helper at {deser_ram:#x}",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0203D76A, arm9_off, arm9_ram),
            b"\xff\xf7\x47\xfa",  # bl 0x203cbfc
            encode_thumb_bl(0x0203D76A, deser_ram),
            "ItemBag::Deserialize: auto-upgrade loaded capacity to max_capacity",
        )

        brac_ram = 0x020CA0E0
        brac_code = build_itembag_bracelet_check_helper()
        patch_bytes(
            rom,
            ram_to_file(brac_ram, arm9_off, arm9_ram),
            b"\x00" * len(brac_code),
            brac_code,
            f"Install ItemBag bracelet bounds-check helper at {brac_ram:#x}",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0203D84A, arm9_off, arm9_ram),
            b"\x09\x58\x01\x90\x18\x29\x16\xd0",  # ldr r1,[r1,r0]; str r0,[sp,#4]; cmp r1,#0x18; beq
            b"\x01\x90" + encode_thumb_bl(0x0203D84C, brac_ram) + b"\x16\xd2",  # str r0,[sp,#4]; bl helper; bhs
            "ItemBag::Deserialize: bounds-checked bracelet index",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0203D8D4, arm9_off, arm9_ram),
            b"\x18\x21",  # movs r1, #0x18
            b"\xff\x21",  # movs r1, #0xff
            "ItemBag::Serialize: no-bracelet sentinel -> 0xFF",
        )

    # 5. L/R/X Dialogue Skip (1-frame delay) + L/R WaitFrames opening/cutscene skip
    if not args.no_lr_skip:
        print("\n[5] Applying L/R/X conversation & opening/cutscene skip patch...")
        patch_bytes(
            rom,
            ram_to_file(0x0201C3AA, arm9_off, arm9_ram),
            b"\x28\x78",  # ldrb r0, [r5]
            b"\x0c\xe0",  # b #0x201c3c6
            "IsDialogueSkipHeld: enable in all conversations & scenes",
        )
        patch_bytes(
            rom,
            ram_to_file(0x0201C3F0, arm9_off, arm9_ram),
            b"\x01\x20\x80\x02",  # 0x0400 (X)
            b"\x07\x20\x00\x02",  # 0x0700 (R | L | X)
            "IsDialogueSkipHeld: accept L, R, or X button",
        )
        for idx, addr in enumerate((0x020555FA, 0x02055782, 0x020559CC, 0x02055D94), 1):
            patch_bytes(
                rom,
                ram_to_file(addr, arm9_off, arm9_ram),
                b"\x1e\x20",  # movs r0, #0x1e
                b"\x01\x20",  # movs r0, #1
                f"Dialogue skip pause #{idx} ({addr:#x}): 30 frames -> 1 frame",
            )
        wf_ram = 0x020CA0F0
        wf_code = build_waitframes_skip_helper()
        patch_bytes(
            rom,
            ram_to_file(wf_ram, arm9_off, arm9_ram),
            b"\x00" * len(wf_code),
            wf_code,
            f"Install WaitFrames L/R cutscene-skip helper at {wf_ram:#x}",
        )
        patch_bytes(
            rom,
            ram_to_file(0x02008F62, arm9_off, arm9_ram),
            b"\x05\x1c\x00\x24",  # adds r5, r0, #0; movs r4, #0
            encode_thumb_bl(0x02008F62, wf_ram),
            "Hook WaitFrames (0x02008F62) for L/R opening/cutscene skip",
        )

    # 6. Pad to 64 MiB and update CRCs
    used_size = len(rom)
    target_size = 0x04000000
    if used_size > target_size:
        raise RuntimeError(f"Patched ROM ({used_size:#x}) exceeds 64 MiB!")
    struct.pack_into("<I", rom, 0x80, used_size)
    if used_size < target_size:
        rom.extend(b"\xFF" * (target_size - used_size))
        print(f"\n[6] Updated header rom_size={used_size:#x} and padded to 64 MiB")

    sec_crc = crc16_nds(bytes(rom[0x4000:0x8000]))
    struct.pack_into("<H", rom, 0x06C, sec_crc)
    hdr_crc = crc16_nds(bytes(rom[0x0000:0x015E]))
    struct.pack_into("<H", rom, 0x15E, hdr_crc)
    print(f"[7] Updated Secure CRC16={sec_crc:#06x}, Header CRC16={hdr_crc:#06x}")

    os.makedirs(os.path.dirname(os.path.abspath(args.output_nds)), exist_ok=True)
    with open(args.output_nds, "wb") as f:
        f.write(rom)
    print(f"\nWrote patched ROM: {args.output_nds} ({len(rom)} bytes)")


if __name__ == "__main__":
    main()
