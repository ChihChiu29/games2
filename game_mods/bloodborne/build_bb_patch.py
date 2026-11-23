#!/usr/bin/env python3
"""Build Bloodborne (PS4 CUSA03173 v1.09 / Lantern Engine / shadPS4) Natural HP Regen patch.

Verified against bloodborne_eboot.bin (unencrypted SELF, SEG[1] off=0x28eb0, VA=0x0):
  - Hook VA        = 0x1A66B10 (CSChrUpdatePostFragment::Execute, 8B: mov rax,[rbx]; mov rdi,rbx; call [rax])
  - Code Cave VA   = 0x130F2B0 (448B dead debug-print func baseMaxHp:[%d])
  - WorldChrMan VA = 0x553E878 -> +0x60 = PlayerIns* -> +0x3B0 = ModuleBag* -> +0x20 = SprjChrDataModule*
  - DataModule     = +0xF8 (HP), +0xFC (MaxHP)
  - ChrIns::SetHp  = 0x18BCD40 (rdi=PlayerIns*, esi=newHp)
  - BSS counter VA = 0x56D30D0 (4-byte frame counter at end of PH[1] .bss)

Regen rate: every 20 frames (~3x/sec @ 60fps, ~1.5x/sec @ 30fps), heals max(1, maxHp / 150) (~2%/sec @ 60fps).
"""
import os
import struct
import capstone
import keystone

EBOOT_SEG1_OFF = 0x28EB0
HOOK_VA        = 0x1A66B10
ORIG_HOOK_HEX  = "488b034889dfff10"  # mov rax,[rbx]; mov rdi,rbx; call qword ptr [rax]
CAVE_VA        = 0x130F2B0
CAVE_MAX       = 0x1C0               # 448 bytes (0x130F2B0..0x130F470)
WCM_VA         = 0x553E878
SETHP_VA       = 0x18BCD40
BSS_CNT_VA     = 0x56D30D0

ASM = f"""
    // 1. Run original 3 instructions at 0x1A66B10 (stack already 16B-aligned after caller's call)
    push rbx
    mov rax, qword ptr [rbx]
    mov rdi, rbx
    call qword ptr [rax]

    // 2. Resolve WorldChrMan -> PlayerIns -> ModuleBag -> SprjChrDataModule
    lea rax, [rip + ({WCM_VA} - $ - 7)]
    mov rax, qword ptr [rax]
    test rax, rax
    jz done
    mov rbx, qword ptr [rax + 0x60]       // PlayerIns*
    test rbx, rbx
    jz done
    mov rcx, qword ptr [rbx + 0x3b0]      // ModuleBag*
    test rcx, rcx
    jz done
    mov rcx, qword ptr [rcx + 0x20]       // SprjChrDataModule*
    test rcx, rcx
    jz done

    // 3. Only regenerate while alive and below MaxHP (hp > 0 && hp < maxHp)
    mov eax, dword ptr [rcx + 0xf8]       // hp
    test eax, eax
    jle done
    mov edx, dword ptr [rcx + 0xfc]       // maxHp
    cmp eax, edx
    jge done

    // 4. Increment frame counter in .bss; heal every 20 frames (~3x/sec @ 60fps)
    lea r8, [rip + ({BSS_CNT_VA} - $ - 7)]
    mov r9d, dword ptr [r8]
    inc r9d
    cmp r9d, 20
    jl store_cnt
    xor r9d, r9d
    mov dword ptr [r8], r9d

    // 5. heal = max(1, maxHp / 150) (~2% MaxHP/sec @ 60fps)
    mov r10d, eax                         // save curHp
    mov r11d, edx                         // save maxHp
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


def encode_call_rel32(src_va, dst_va):
    rel = dst_va - (src_va + 5)
    return b"\xe8" + struct.pack("<i", rel)


def main():
    # Assemble cave with Keystone (resolve RIP-relative manually via two-pass or exact offsets)
    # Note: Keystone handles absolute labels and `rip + imm` when using symbol addresses directly!
    # Let's use Keystone's native `qword ptr [rip + sym]` or compute exact RIP displacements cleanly.
    asm_clean = ASM
    # Replace ($ - ...) expressions with exact RIP-relative placeholders? Simpler: assemble with keystone using labels!
    ks = keystone.Ks(keystone.KS_ARCH_X86, keystone.KS_MODE_64)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

    # First pass with dummy disp32=0 to get exact instruction addresses for WCM_VA and BSS_CNT_VA
    def build_asm(wcm_disp, bss_disp):
        return f"""
            push rbx
            mov rax, qword ptr [rbx]
            mov rdi, rbx
            call qword ptr [rax]

            mov rax, qword ptr [rip + {wcm_disp}]
            test rax, rax
            jz done
            mov rbx, qword ptr [rax + 0x60]
            test rbx, rbx
            jz done
            mov rcx, qword ptr [rbx + 0x3b0]
            test rcx, rcx
            jz done
            mov rcx, qword ptr [rcx + 0x20]
            test rcx, rcx
            jz done

            mov eax, dword ptr [rcx + 0xf8]
            test eax, eax
            jle done
            mov edx, dword ptr [rcx + 0xfc]
            cmp eax, edx
            jge done

            lea r8, [rip + {bss_disp}]
            mov r9d, dword ptr [r8]
            inc r9d
            cmp r9d, 20
            jl store_cnt
            xor r9d, r9d
            mov dword ptr [r8], r9d

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

    hook_bytes = encode_call_rel32(HOOK_VA, CAVE_VA) + b"\x90\x90\x90"
    assert len(hook_bytes) == 8

    print(f"Bloodborne cave: {cnt} instructions ({len(cave_bytes)}/{CAVE_MAX} bytes) at {CAVE_VA:#x}")
    for ins in md.disasm(cave_bytes, CAVE_VA):
        print(f"  {ins.address:#x}: {ins.mnemonic:8s} {ins.op_str}")
    print(f"Hook @ {HOOK_VA:#x}: {hook_bytes.hex()}")

    # Verify against local bloodborne_eboot.bin if present
    eboot_path = "/home/chih/Workspace/git/games2/game_mods/bloodborne/bloodborne_eboot.bin"
    if os.path.exists(eboot_path):
        with open(eboot_path, "rb") as f:
            f.seek(EBOOT_SEG1_OFF + HOOK_VA)
            orig = f.read(8)
        assert orig == bytes.fromhex(ORIG_HOOK_HEX), f"Hook mismatch: {orig.hex()} != {ORIG_HOOK_HEX}"
        print("Verified original bytes in bloodborne_eboot.bin!")

    # Write standalone patcher script into release/
    out_dir = "/home/chih/Workspace/git/games2/game_mods/bloodborne/release"
    os.makedirs(out_dir, exist_ok=True)
    patcher_path = os.path.join(out_dir, "patch_bloodborne_eboot.py")
    patcher_code = f'''#!/usr/bin/env python3
"""Patch Bloodborne (CUSA03173 v1.09) eboot.bin with Natural HP Regen (~2% MaxHP/sec @ 60fps).

Usage:
    python patch_bloodborne_eboot.py /path/to/eboot.bin
    python patch_bloodborne_eboot.py --restore /path/to/eboot.bin
"""
import os
import shutil
import sys

SEG1_OFF  = {EBOOT_SEG1_OFF:#x}
HOOK_VA   = {HOOK_VA:#x}
ORIG_HOOK = bytes.fromhex("{ORIG_HOOK_HEX}")
NEW_HOOK  = bytes.fromhex("{hook_bytes.hex()}")
CAVE_VA   = {CAVE_VA:#x}
CAVE_DATA = bytes.fromhex("{cave_bytes.hex()}")


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
