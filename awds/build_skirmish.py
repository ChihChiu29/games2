#!/usr/bin/env python3
"""
Build & ROM Patcher for Advance Wars: Dual Strike (EU AWRP00) Procedural Map Generator.
Compiles skirmish_hooks.s + skirmish_ds.c, repacks OV9 5 map LZ77 blobs, injects
the procedural generator payload at 0x0233E950, patches GetMapName and DecompressMap
in ARM9, and outputs /home/chih/Workspace/git/games2/awds/awds_skirmish.nds.
"""

import os
import struct
import subprocess
import tempfile

BASE_DIR = "/home/chih/Workspace/git/games2/awds"
BASEROM_PATH = os.path.join(BASE_DIR, "awds-decomp/extract/baserom_awds_SWRP00.nds")
OUTROM_PATH = os.path.join(BASE_DIR, "awds_skirmish.nds")
SRC_C = os.path.join(BASE_DIR, "awds-decomp/src/skirmish_ds.c")
SRC_S = os.path.join(BASE_DIR, "awds-decomp/src/skirmish_hooks.s")

TOOLCHAIN_BIN = "/home/chih/.gemini/antigravity-cli/brain/126ba518-2be9-4c55-a9a8-bf1a21337246/scratch/dkp/sysroot/usr/bin"
GCC = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-gcc")
AS = os.path.join(TOOLCHAIN_BIN, "arm-none-eabi-as")
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
REPACK_END_RAM = 0x02347C5C

SKIRMISH_MAP_IDS = (
    set(range(1, 6))
    | set(range(18, 23))
    | set(range(76, 81))
    | set(range(103, 108))
)


def arm_bl(src_vma: int, dst_vma: int) -> int:
    diff = dst_vma - (src_vma + 8)
    assert diff % 4 == 0, f"Unaligned branch target: 0x{src_vma:08X} -> 0x{dst_vma:08X}"
    imm24 = (diff >> 2) & 0x00FFFFFF
    return 0xEB000000 | imm24


def lz77_padded_len(buf: bytes, off: int) -> int:
    hdr = struct.unpack_from("<I", buf, off)[0]
    size = hdr >> 8
    out_len = 0
    pos = off + 4
    while out_len < size:
        flags = buf[pos]
        pos += 1
        for i in range(7, -1, -1):
            if out_len >= size:
                break
            if flags & (1 << i):
                b1 = buf[pos]
                pos += 2
                out_len += (b1 >> 4) + 3
            else:
                pos += 1
                out_len += 1
    while (pos - off) % 4 != 0:
        pos += 1
    return pos - off


def compile_payload() -> tuple[bytes, dict[str, int]]:
    with tempfile.TemporaryDirectory() as tmpdir:
        hooks_o = os.path.join(tmpdir, "skirmish_hooks.o")
        c_o = os.path.join(tmpdir, "skirmish_ds.o")
        elf_path = os.path.join(tmpdir, "skirmish_payload.elf")
        bin_path = os.path.join(tmpdir, "skirmish_payload.bin")
        ld_script = os.path.join(tmpdir, "payload.ld")

        subprocess.run(
            [AS, "-mcpu=arm946e-s", "-mthumb-interwork", SRC_S, "-o", hooks_o],
            check=True,
        )
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

        subprocess.run([LD, "-T", ld_script, hooks_o, c_o, "-o", elf_path], check=True)
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
    print(f"      Payload size: {len(payload)} bytes (0x{len(payload):X})")
    print(f"      HookGetMapName:    0x{hook_get_name:08X}")
    print(f"      HookDecompressMap: 0x{hook_decomp:08X}")

    with open(BASEROM_PATH, "rb") as f:
        rom = bytearray(f.read())

    ov5 = bytearray(rom[OV5_ROM_OFF : OV5_ROM_OFF + OV5_SIZE])
    map_table_off = MAP_TABLE_RAM - OV5_RAM_BASE

    print("[2/4] Repacking OV9 5 LZ77 map blobs (maps 1..107)...")
    kept_blobs = []
    for map_id in range(1, 108):
        if map_id in SKIRMISH_MAP_IDS:
            continue
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        lz_ptr = struct.unpack_from("<I", ov5, e_off + 0x44)[0]
        blob_off = lz_ptr - OV5_RAM_BASE
        blob_len = lz77_padded_len(ov5, blob_off)
        kept_blobs.append((map_id, bytes(ov5[blob_off : blob_off + blob_len])))

    total_kept_bytes = sum(len(b[1]) for b in kept_blobs)
    repack_start_ram = REPACK_END_RAM - total_kept_bytes
    max_payload_bytes = repack_start_ram - PAYLOAD_RAM_BASE
    print(
        f"      Available code space at 0x{PAYLOAD_RAM_BASE:08X}..0x{repack_start_ram:08X}: "
        f"{max_payload_bytes} bytes"
    )
    assert len(payload) <= max_payload_bytes, (
        f"Payload ({len(payload)} B) exceeds available space ({max_payload_bytes} B)!"
    )

    # Clear 0x0233E950..0x02347C5C and write payload + repacked LZ77 blobs
    region_off = PAYLOAD_RAM_BASE - OV5_RAM_BASE
    region_len = REPACK_END_RAM - PAYLOAD_RAM_BASE
    ov5[region_off : region_off + region_len] = b"\x00" * region_len
    ov5[region_off : region_off + len(payload)] = payload

    cur_ram = repack_start_ram
    for map_id, blob in kept_blobs:
        dest_off = cur_ram - OV5_RAM_BASE
        ov5[dest_off : dest_off + len(blob)] = blob
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        struct.pack_into("<I", ov5, e_off + 0x44, cur_ram)
        cur_ram += len(blob)
    assert cur_ram == REPACK_END_RAM

    print("[3/4] Updating MapTable entries for the 20 Skirmish difficulty slots...")
    for map_id in SKIRMISH_MAP_IDS:
        e_off = map_table_off + map_id * MAP_ENTRY_SIZE
        # Unlock map by default (f18 = 0x00000001) and clear shop lock flag (f40 = 0x000000FF)
        struct.pack_into("<I", ov5, e_off + 0x18, 0x00000001)
        struct.pack_into("<I", ov5, e_off + 0x40, 0x000000FF)
        struct.pack_into("<I", ov5, e_off + 0x44, PAYLOAD_RAM_BASE)

    rom[OV5_ROM_OFF : OV5_ROM_OFF + OV5_SIZE] = ov5

    print("[4/4] Patching ARM9 hooks at 0x020C6874 and 0x020C6A80...")
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
