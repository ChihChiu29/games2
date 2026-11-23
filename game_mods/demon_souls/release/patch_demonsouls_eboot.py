#!/usr/bin/env python3
"""Patch Demon's Souls (PS5 Remake / KytyPS5) eboot.bin with Natural HP Regen (~2% MaxHP/sec @ 60fps).

Usage:
    python patch_demonsouls_eboot.py /path/to/eboot.bin
    python patch_demonsouls_eboot.py --restore /path/to/eboot.bin
"""
import os
import shutil
import sys

SEG1_OFF  = 0xfe50
HOOK_VA   = 0x34da9e
ORIG_HOOK = bytes.fromhex("e82dfaffff")
NEW_HOOK  = bytes.fromhex("e85d88ecff")
CAVE_VA   = 0x216300
CAVE_DATA = bytes.fromhex("53e8ca711300488b056b905b024885c0746b488b98580200004885db745f8b831007000085c07e558b931807000039d07d4b4c8d0567b41503458b0841ffc14183f9147c354531c94589084189c24189d389d099b996000000f7f985c07f05b8010000004401d04439d8410f4fc34889df89c6e818f51000eb034589085bc3")


def main():
    args = sys.argv[1:]
    restore = False
    if args and args[0] == "--restore":
        restore = True
        args = args[1:]
    if len(args) != 1:
        print("Usage: python patch_demonsouls_eboot.py [--restore] <path_to_eboot.bin>")
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
