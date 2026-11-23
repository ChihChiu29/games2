#!/usr/bin/env python3
"""Patch Bloodborne (CUSA03173 v1.09) eboot.bin with Natural HP Regen (~2% MaxHP/sec @ 60fps).

Usage:
    python patch_bloodborne_eboot.py /path/to/eboot.bin
    python patch_bloodborne_eboot.py --restore /path/to/eboot.bin
"""
import os
import shutil
import sys

SEG1_OFF  = 0x28eb0
HOOK_VA   = 0x1a66b10
ORIG_HOOK = bytes.fromhex("488b034889dfff10")
NEW_HOOK  = bytes.fromhex("e89b878aff909090")
CAVE_VA   = 0x130f2b0
CAVE_DATA = bytes.fromhex("53488b034889dfff10488b05b8f522044885c0747d488b58604885db7474488b8bb00300004885c97468488b49204885c9745f8b81f800000085c07e558b91fc00000039d07d4b4c8d05d23d3c04458b0841ffc14183f9147c354531c94589084189c24189d389d099b996000000f7f985c07f05b8010000004401d04439d8410f4fc34889df89c6e803da5a00eb034589085bc3")


def main():
    args = sys.argv[1:]
    restore = False
    if args and args[0] == "--restore":
        restore = True
        args = args[1:]
    if len(args) != 1:
        print("Usage: python patch_bloodborne_eboot.py [--restore] <path_to_eboot.bin>")
        sys.exit(1)

    path = args[0]
    bak = path + ".bak"

    if restore:
        if not os.path.exists(bak):
            print(f"Backup not found: {bak}")
            sys.exit(1)
        shutil.copy2(bak, path)
        print(f"Restored {path} from {bak}")
        return

    with open(path, "r+b") as f:
        f.seek(SEG1_OFF + HOOK_VA)
        cur = f.read(len(ORIG_HOOK))
        if cur == NEW_HOOK:
            print("Already patched!")
            return
        if cur != ORIG_HOOK:
            print(f"ERROR: Unexpected bytes at hook {HOOK_VA:#x}: {cur.hex()} (expected {ORIG_HOOK.hex()})")
            sys.exit(1)

        if not os.path.exists(bak):
            shutil.copy2(path, bak)
            print(f"Created backup: {bak}")

        f.seek(SEG1_OFF + CAVE_VA)
        f.write(CAVE_DATA)
        f.seek(SEG1_OFF + HOOK_VA)
        f.write(NEW_HOOK)

    print(f"Successfully patched {path} with Natural HP Regen!")


if __name__ == "__main__":
    main()
