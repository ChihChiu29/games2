#!/usr/bin/env python3
"""Build Demon's Souls (PS5 Remake / KytyPS5) Natural HP Regen patch.

Verified against demon_eboot.bin (unencrypted PS5 SELF, SEG[1] off=0xFE50, VA=0x0):
  - Hook VA        = 0x34DA9E (call 0x34D4D0 = DemonsSoul::ChrInsMan::_UpdateChr_ByListsPre)
  - Code Cave VA   = 0x216300 (224B dead debug console command, 0 direct callers)
  - WorldChrMan VA = 0x27CF378 -> +0x258 = PlayerIns*
  - PlayerIns      = +0x710 (HP), +0x718 (MaxHP)
  - ChrIns::SetHp  = 0x325890 (rdi=PlayerIns*, esi=newHp)
  - BSS counter VA = 0x33717A0 (4-byte frame counter at end of PH[7] .bss)

Regen rate: every 20 frames (~3x/sec @ 60fps, ~1.5x/sec @ 30fps), heals max(1, maxHp / 150) (~2%/sec @ 60fps).
"""
import os
import struct
import capstone
import keystone

EBOOT_SEG1_OFF = 0xFE50
HOOK_VA        = 0x34DA9E
ORIG_TARGET    = 0x34D4D0
CAVE_VA        = 0x216300
CAVE_MAX       = 0xE0                # 224 bytes (0x216300..0x2163E0)
WCM_VA         = 0x27CF378
SETHP_VA       = 0x325890
BSS_CNT_VA     = 0x33717A0


def encode_call_rel32(src_va, dst_va):
    rel = dst_va - (src_va + 5)
    return b"\xe8" + struct.pack("<i", rel)


def main():
    ks = keystone.Ks(keystone.KS_ARCH_X86, keystone.KS_MODE_64)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

    def build_asm(wcm_disp, bss_disp):
        return f"""
            // 1. Call original _UpdateChr_ByListsPre with incoming args intact (stack aligned via push rbx)
            push rbx
            call {ORIG_TARGET}

            // 2. Resolve WorldChrMan -> PlayerIns
            mov rax, qword ptr [rip + {wcm_disp}]
            test rax, rax
            jz done
            mov rbx, qword ptr [rax + 0x258]
            test rbx, rbx
            jz done

            // 3. Only regenerate while alive and below MaxHP (hp > 0 && hp < maxHp)
            mov eax, dword ptr [rbx + 0x710]
            test eax, eax
            jle done
            mov edx, dword ptr [rbx + 0x718]
            cmp eax, edx
            jge done

            // 4. Increment frame counter in .bss; heal every 20 frames (~3x/sec @ 60fps)
            lea r8, [rip + {bss_disp}]
            mov r9d, dword ptr [r8]
            inc r9d
            cmp r9d, 20
            jl store_cnt
            xor r9d, r9d
            mov dword ptr [r8], r9d

            // 5. heal = max(1, maxHp / 150) (~2% MaxHP/sec @ 60fps)
            mov r10d, eax
            mov r11d, edx
            mov eax, edx
            cdq
            mov ecx, 150
            idiv ecx
            test eax, eax
            jg has_heal
            mov eax, 1
        has_heal:
            add eax, r10d
            cmp eax, r11d
            cmovg eax, r11d

            // 6. Call ChrIns::SetHp(PlayerIns* rdi, int32_t esi)
            mov rdi, rbx
            mov esi, eax
            call {SETHP_VA}
            jmp done

        store_cnt:
            mov dword ptr [r8], r9d

        done:
            pop rbx
            ret
        """

    enc0, _ = ks.asm(build_asm(0x11111111, 0x22222222), CAVE_VA)
    b0 = bytes(enc0)
    wcm_ins_end = None
    bss_ins_end = None
    for ins in md.disasm(b0, CAVE_VA):
        if "0x11111111" in ins.op_str:
            wcm_ins_end = ins.address + ins.size
        elif "0x22222222" in ins.op_str:
            bss_ins_end = ins.address + ins.size
    assert wcm_ins_end and bss_ins_end

    enc, cnt = ks.asm(build_asm(WCM_VA - wcm_ins_end, BSS_CNT_VA - bss_ins_end), CAVE_VA)
    cave_bytes = bytes(enc)
    assert len(cave_bytes) <= CAVE_MAX, f"Cave overflow: {len(cave_bytes)} > {CAVE_MAX}"

    orig_hook = encode_call_rel32(HOOK_VA, ORIG_TARGET)
    new_hook = encode_call_rel32(HOOK_VA, CAVE_VA)

    print(f"Demon's Souls PS5 cave: {cnt} instructions ({len(cave_bytes)}/{CAVE_MAX} bytes) at {CAVE_VA:#x}")
    for ins in md.disasm(cave_bytes, CAVE_VA):
        print(f"  {ins.address:#x}: {ins.mnemonic:8s} {ins.op_str}")
    print(f"Hook @ {HOOK_VA:#x}: {orig_hook.hex()} -> {new_hook.hex()}")

    eboot_path = "/home/chih/Workspace/git/games2/game_mods/demon_souls/demon_eboot.bin"
    if os.path.exists(eboot_path):
        with open(eboot_path, "rb") as f:
            f.seek(EBOOT_SEG1_OFF + HOOK_VA)
            orig = f.read(5)
        assert orig == orig_hook, f"Hook mismatch: {orig.hex()} != {orig_hook.hex()}"
        print("Verified original bytes in demon_eboot.bin!")

    out_dir = "/home/chih/Workspace/git/games2/game_mods/demon_souls/release"
    os.makedirs(out_dir, exist_ok=True)
    patcher_path = os.path.join(out_dir, "patch_demonsouls_eboot.py")
    patcher_code = f'''#!/usr/bin/env python3
"""Patch Demon's Souls (PS5 Remake / KytyPS5) eboot.bin with Natural HP Regen (~2% MaxHP/sec @ 60fps).

Usage:
    python patch_demonsouls_eboot.py /path/to/eboot.bin
    python patch_demonsouls_eboot.py --restore /path/to/eboot.bin
"""
import os
import shutil
import sys

SEG1_OFF  = {EBOOT_SEG1_OFF:#x}
HOOK_VA   = {HOOK_VA:#x}
ORIG_HOOK = bytes.fromhex("{orig_hook.hex()}")
NEW_HOOK  = bytes.fromhex("{new_hook.hex()}")
CAVE_VA   = {CAVE_VA:#x}
CAVE_DATA = bytes.fromhex("{cave_bytes.hex()}")


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
            print(f"Backup not found: {{bak}}")
            sys.exit(1)
        shutil.copy2(bak, path)
        print(f"Restored {{path}} from {{bak}}")
        return

    with open(path, "r+b") as f:
        f.seek(SEG1_OFF + HOOK_VA)
        cur = f.read(len(ORIG_HOOK))
        if cur == NEW_HOOK:
            print("Already patched!")
            return
        if cur != ORIG_HOOK:
            print(f"ERROR: Unexpected bytes at hook {{HOOK_VA:#x}}: {{cur.hex()}} (expected {{ORIG_HOOK.hex()}})")
            sys.exit(1)

        if not os.path.exists(bak):
            shutil.copy2(path, bak)
            print(f"Created backup: {{bak}}")

        f.seek(SEG1_OFF + CAVE_VA)
        f.write(CAVE_DATA)
        f.seek(SEG1_OFF + HOOK_VA)
        f.write(NEW_HOOK)

    print(f"Successfully patched {{path}} with Natural HP Regen!")


if __name__ == "__main__":
    main()
'''
    with open(patcher_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(patcher_code)
    os.chmod(patcher_path, 0o755)
    print(f"\nWrote: {patcher_path}")


if __name__ == "__main__":
    main()
