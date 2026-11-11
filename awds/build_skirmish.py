#!/usr/bin/env python3
"""
Build & ROM Patcher for Advance Wars: Dual Strike (EU AWRP00) Procedural Map Generator.
Compiles skirmish_ds.c, repacks OV9 5 map LZ77 blobs (preserving all 187 Versus maps
and Campaign/Tutorial maps while reclaiming the 36 unused cat=9 blobs), injects the
procedural generator payload at 0x0233E950, configures the dedicated Random tab
(tabId=7, cat=9, mid=188..198) with all 11 Skirmish modes, patches ARM9 hooks,
and outputs /home/chih/Workspace/git/games2/awds/awds_skirmish.nds.
"""

import os
import struct
import subprocess
import tempfile

BASE_DIR = "/home/chih/Workspace/git/games2/awds"
BASEROM_PATH = os.path.join(BASE_DIR, "awds-decomp/extract/baserom_awds_SWRP00.nds")
OUTROM_PATH = os.path.join(BASE_DIR, "awds_skirmish.nds")
SRC_C = os.path.join(BASE_DIR, "awds-decomp/src/skirmish_ds.c")

TOOLCHAIN_BIN = "/home/chih/.gemini/antigravity-cli/brain/126ba518-2be9-4c55-a9a8-bf1a21337246/scratch/dkp/sysroot/usr/bin"
GCC = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-gcc")
LD = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-ld")
OBJCOPY = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-objcopy")
NM = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-nm")

ARM9_ROM_OFF = 0x4000
ARM9_RAM_BASE = 0x02000000

OV5_ROM_OFF = 0x359000
OV5_RAM_BASE = 0x023384C0
OV5_SIZE = 367236

MAP_TABLE_RAM = 0x0235DF88
MAP_ENTRY_SIZE = 0xA0

PAYLOAD_RAM_BASE = 0x0233E950
REPACK_END_RAM = 0x02354350

SKIRMISH_MAP_BASE = 188
SKIRMISH_MODES_ARMIES = [2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4]
REPLACED_CAT9_IDS = set(range(188, 224))


def arm_bl(src_vma: int, dst_vma: int) -> int:
    diff = dst_vma - (src_vma + 8)
    assert diff % 4 == 0, f"Unaligned branch target: 0x{src_vma:08X} -> 0x{dst_vma:08X}"
    imm24 = (diff >> 2) & 0x00FFFFFF
    return 0xEB000000 | imm24


def lz77_decompress(buf: bytes, off: int) -> bytes:
    hdr = struct.unpack_from("<I", buf, off)[0]
    assert (hdr & 0xFF) == 0x10, f"Invalid LZ77 header at 0x{off:X}: 0x{hdr:08X}"
    size = hdr >> 8
    out = bytearray()
    pos = off + 4
    while len(out) < size:
        flags = buf[pos]
        pos += 1
        for i in range(7, -1, -1):
            if len(out) >= size:
                break
            if flags & (1 << i):
                b1 = buf[pos]
                b2 = buf[pos + 1]
                pos += 2
                length = (b1 >> 4) + 3
                disp = ((b1 & 0x0F) << 8) | b2
                src = len(out) - (disp + 1)
                for _ in range(length):
                    out.append(out[src])
                    src += 1
            else:
                out.append(buf[pos])
                pos += 1
    return bytes(out[:size])


def lz77_compress_optimal(data: bytes) -> bytes:
    n = len(data)
    best_len = [1] * n
    best_disp = [0] * n
    for i in range(n):
        max_l = min(18, n - i)
        if max_l < 3:
            continue
        start_j = max(0, i - 4096)
        b_l = 0
        b_d = 0
        for l in range(max_l, 2, -1):
            sub = data[i : i + l]
            idx = data.find(sub, start_j, i + l - 1)
            if idx != -1 and idx < i:
                b_l = l
                b_d = i - idx - 1
                break
        if b_l >= 3:
            best_len[i] = b_l
            best_disp[i] = b_d

    dp = [10**9] * (n + 1)
    choice = [1] * n
    dp[n] = 0
    for i in range(n - 1, -1, -1):
        c1 = 9 + dp[i + 1]
        best_c = c1
        best_step = 1
        ml = best_len[i]
        for l in range(3, ml + 1):
            cl = 17 + dp[i + l]
            if cl < best_c:
                best_c = cl
                best_step = l
        dp[i] = best_c
        choice[i] = best_step

    out = bytearray(struct.pack("<I", 0x10 | (n << 8)))
    i = 0
    while i < n:
        flag_pos = len(out)
        out.append(0)
        flags = 0
        for bit in range(7, -1, -1):
            if i >= n:
                break
            step = choice[i]
            if step >= 3:
                flags |= 1 << bit
                disp = best_disp[i]
                b1 = ((step - 3) << 4) | ((disp >> 8) & 0x0F)
                b2 = disp & 0xFF
                out.append(b1)
                out.append(b2)
                i += step
            else:
                out.append(data[i])
                i += 1
        out[flag_pos] = flags
    while len(out) % 4 != 0:
        out.append(0)
    return bytes(out)


def compile_payload() -> tuple[bytes, dict[str, int]]:
    with tempfile.TemporaryDirectory() as tmpdir:
        c_o = os.path.join(tmpdir, "skirmish_ds.o")
        elf_path = os.path.join(tmpdir, "skirmish_payload.elf")
        bin_path = os.path.join(tmpdir, "skirmish_payload.bin")
        ld_script = os.path.join(tmpdir, "payload.ld")

        subprocess.run(
            [
                GCC,
                "-Os",
                "-fno-jump-tables",
                "-mcpu=arm946e-s",
                "-mthumb",
                "-mthumb-interwork",
                "-ffreestanding",
                "-fno-builtin",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-c",
                SRC_C,
                "-o",
                c_o,
            ],
            check=True,
        )

        with open(ld_script, "w") as f:
            f.write(
                f"""
SECTIONS {{
    . = 0x{PAYLOAD_RAM_BASE:08X};
    .text : {{
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    }}
    .data : {{
        *(.data*)
        *(.bss*)
        . = ALIGN(4);
    }}
}}
"""
            )

        subprocess.run([LD, "-T", ld_script, c_o, "-o", elf_path], check=True)
        subprocess.run([OBJCOPY, "-O", "binary", elf_path, bin_path], check=True)

        nm_out = subprocess.run(
            [NM, elf_path], capture_output=True, text=True, check=True
        ).stdout
        syms = {}
        for line in nm_out.splitlines():
            parts = line.split()
            if len(parts) == 3:
                syms[parts[2]] = int(parts[0], 16)

        with open(bin_path, "rb") as f:
            payload = f.read()

        return payload, syms


def main():
    print("[1/4] Compiling Skirmish C + ARM assembly payload...")
    payload, syms = compile_payload()
    hook_get_name = syms["HookGetMapName"]
    hook_decomp = syms["HookDecompressMap"]
    hook_is_cat_unlocked = syms["HookIsCategoryUnlocked"]
    print(f"      Payload size:           {len(payload)} bytes (0x{len(payload):X})")
    print(f"      HookGetMapName:         0x{hook_get_name:08X}")
    print(f"      HookDecompressMap:      0x{hook_decomp:08X}")
    print(f"      HookIsCategoryUnlocked: 0x{hook_is_cat_unlocked:08X}")

    with open(BASEROM_PATH, "rb") as f:
        rom = bytearray(f.read())

    ov5 = bytearray(rom[OV5_ROM_OFF : OV5_ROM_OFF + OV5_SIZE])
    map_table_off = MAP_TABLE_RAM - OV5_RAM_BASE

    print("[2/4] Repacking OV9 5 LZ77 map blobs (preserving all 187 Versus + Campaign maps)...")
    raw_to_comp: dict[bytes, bytes] = {}
    unique_blobs: list[bytes] = []
    ptr_refs: list[tuple[int, int, bytes]] = []

    for map_id in range(1, 257):
        if map_id in REPLACED_CAT9_IDS:
            continue
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        for field_off in (0x44, 0x48):
            lz_ptr = struct.unpack_from("<I", ov5, e_off + field_off)[0]
            if PAYLOAD_RAM_BASE <= lz_ptr < REPACK_END_RAM:
                blob_off = lz_ptr - OV5_RAM_BASE
                raw = lz77_decompress(ov5, blob_off)
                if raw not in raw_to_comp:
                    comp = lz77_compress_optimal(raw)
                    assert lz77_decompress(comp, 0) == raw
                    raw_to_comp[raw] = comp
                    unique_blobs.append(comp)
                ptr_refs.append((map_id, field_off, raw))

    total_kept_bytes = sum(len(b) for b in unique_blobs)
    repack_start_ram = REPACK_END_RAM - total_kept_bytes
    max_payload_bytes = repack_start_ram - PAYLOAD_RAM_BASE
    print(
        f"      Available code space at 0x{PAYLOAD_RAM_BASE:08X}..0x{repack_start_ram:08X}: "
        f"{max_payload_bytes} bytes (headroom: {max_payload_bytes - len(payload)} bytes)"
    )
    assert len(payload) <= max_payload_bytes, (
        f"Payload ({len(payload)} B) exceeds available space ({max_payload_bytes} B)!"
    )

    # Clear 0x0233E950..0x02354350 and write payload + repacked LZ77 blobs
    region_off = PAYLOAD_RAM_BASE - OV5_RAM_BASE
    region_len = REPACK_END_RAM - PAYLOAD_RAM_BASE
    ov5[region_off : region_off + region_len] = b"\x00" * region_len
    ov5[region_off : region_off + len(payload)] = payload

    comp_to_ram: dict[bytes, int] = {}
    cur_ram = repack_start_ram
    for comp in unique_blobs:
        comp_to_ram[comp] = cur_ram
        dest_off = cur_ram - OV5_RAM_BASE
        ov5[dest_off : dest_off + len(comp)] = comp
        cur_ram += len(comp)
    assert cur_ram == REPACK_END_RAM

    for map_id, field_off, raw in ptr_refs:
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        struct.pack_into("<I", ov5, e_off + field_off, comp_to_ram[raw_to_comp[raw]])

    print("[3/4] Configuring dedicated Random Tab (tabId=7, cat=9, mid=188..198)...")
    for idx, num_armies in enumerate(SKIRMISH_MODES_ARMIES):
        map_id = SKIRMISH_MAP_BASE + idx
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        # Zero entire 0xA0-byte entry so no leftover Survival mode scripts/COs remain
        ov5[e_off : e_off + MAP_ENTRY_SIZE] = b"\x00" * MAP_ENTRY_SIZE
        # Unlock map by default (f18 = 1)
        struct.pack_into("<I", ov5, e_off + 0x18, 0x00000001)
        # Set numArmies (+0x24), cat = 9 (+0x26), flags (+0x28, +0x2A), Par Turns = 24 (+0x2C)
        struct.pack_into("<HHHHH", ov5, e_off + 0x24, num_armies, 9, 1, 1, 24)
        # Clear shop lock flag (+0x40 = 0xFF), set valid map pointer (+0x44), clear predeployed units (+0x48..+0x50)
        struct.pack_into("<IIIII", ov5, e_off + 0x40, 0x000000FF, PAYLOAD_RAM_BASE, 0, 0, 0)
        # Standard Versus army order: OS=1, BM=2, GE=3, YC=4
        ov5[e_off + 0x88 : e_off + 0x92] = bytes([0, 1, 2, 3, 4, 0, 1, 2, 3, 4])
        ov5[e_off + 0x94 : e_off + 0x9C] = b"\xFF" * 8

    # Hide remaining unused cat=9 entries (mid=199..223) from tabId=7
    for map_id in range(SKIRMISH_MAP_BASE + len(SKIRMISH_MODES_ARMIES), 224):
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        struct.pack_into("<HH", ov5, e_off + 0x24, 0, 0)

    rom[OV5_ROM_OFF : OV5_ROM_OFF + OV5_SIZE] = ov5

    # Patch res_mapselect_lang_E (0x1BE6A00..0x1BE7B90) Banner 12+13 ("SURVIVAL" -> "RANDOM")
    lang_e_off = 0x1BE6A00
    lang_e_end = 0x1BE7B90
    lang_raw = bytearray(lz77_decompress(rom, lang_e_off))

    def get_128x16(buf: bytearray, pair_idx: int) -> list[list[int]]:
        grid = [[0] * 128 for _ in range(16)]
        for b in range(2):
            s = pair_idx * 2 + b
            for ty in range(2):
                for py in range(8):
                    for half in range(2):
                        for tx in range(4):
                            t_idx = s * 16 + half * 8 + ty * 4 + tx
                            toff = t_idx * 32 + py * 4
                            for bx in range(4):
                                byte = buf[toff + bx]
                                x0 = b * 64 + half * 32 + tx * 8 + bx * 2
                                y0 = ty * 8 + py
                                grid[y0][x0] = byte & 0xF
                                grid[y0][x0 + 1] = byte >> 4
        return grid

    def set_128x16(buf: bytearray, pair_idx: int, grid: list[list[int]]) -> None:
        for b in range(2):
            s = pair_idx * 2 + b
            for ty in range(2):
                for py in range(8):
                    for half in range(2):
                        for tx in range(4):
                            t_idx = s * 16 + half * 8 + ty * 4 + tx
                            toff = t_idx * 32 + py * 4
                            for bx in range(4):
                                x0 = b * 64 + half * 32 + tx * 8 + bx * 2
                                y0 = ty * 8 + py
                                buf[toff + bx] = (grid[y0][x0] & 0xF) | ((grid[y0][x0 + 1] & 0xF) << 4)

    g_dep = get_128x16(lang_raw, 1)  # DEPLOYED
    g_war = get_128x16(lang_raw, 5)  # WAR ROOM
    g_sur = get_128x16(lang_raw, 6)  # SURVIVAL
    g_des = get_128x16(lang_raw, 7)  # DESIGN MAPS

    def extract_cols(g: list[list[int]], x0: int, x1: int) -> list[list[int]]:
        return [[g[y][x] for x in range(x0, x1 + 1)] for y in range(3, 13)]

    glyphs = [
        extract_cols(g_sur, 38, 51),   # R
        extract_cols(g_sur, 92, 105),  # A
        extract_cols(g_des, 58, 69),   # N
        extract_cols(g_dep, 4, 16),    # D
        extract_cols(g_war, 75, 88),   # O
        extract_cols(g_war, 107, 120), # M
    ]
    for y in range(3, 13):
        for x in range(4, 124):
            g_sur[y][x] = 4
    total_w = sum(len(gl[0]) for gl in glyphs) + 3 * (len(glyphs) - 1)
    cur_x = (128 - total_w) // 2
    for gl in glyphs:
        gw = len(gl[0])
        for y in range(10):
            for dx in range(gw):
                g_sur[3 + y][cur_x + dx] = gl[y][dx]
        cur_x += gw + 3
    set_128x16(lang_raw, 6, g_sur)
    lang_comp = lz77_compress_optimal(bytes(lang_raw))
    assert len(lang_comp) <= (lang_e_end - lang_e_off)
    rom[lang_e_off:lang_e_end] = lang_comp.ljust(lang_e_end - lang_e_off, b"\x00")

    print("[4/4] Patching ARM9 hooks (0x0202E0B0, 0x0202E114, 0x020C6874, 0x020C6A80)...")
    # Enable tabId=7 in Versus Mode Map Select (nop out `beq 0x0202E0D8` at 0x0202E0B0)
    off_tab7_skip = 0x0202E0B0 - ARM9_RAM_BASE + ARM9_ROM_OFF
    struct.pack_into("<I", rom, off_tab7_skip, 0xE1A00000)

    # Hook IsCategoryUnlocked call at 0x0202E114
    off_cat_unlock = 0x0202E114 - ARM9_RAM_BASE + ARM9_ROM_OFF
    struct.pack_into("<I", rom, off_cat_unlock, arm_bl(0x0202E114, hook_is_cat_unlocked))

    # Patch GetMapName (0x020C6874..0x020C687C)
    off_get_name = 0x020C6874 - ARM9_RAM_BASE + ARM9_ROM_OFF
    struct.pack_into(
        "<III",
        rom,
        off_get_name,
        arm_bl(0x020C6874, hook_get_name),
        0xE1A00000,  # nop
        0xE1A00000,  # nop
    )

    # Patch DecompressMap (0x020C6A80)
    off_decomp = 0x020C6A80 - ARM9_RAM_BASE + ARM9_ROM_OFF
    struct.pack_into("<I", rom, off_decomp, arm_bl(0x020C6A80, hook_decomp))

    with open(OUTROM_PATH, "wb") as f:
        f.write(rom)

    print(f"Successfully built {OUTROM_PATH} ({len(rom)} bytes)!")


if __name__ == "__main__":
    main()
