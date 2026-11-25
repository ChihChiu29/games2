#!/usr/bin/env python3
"""Generate self-contained release/patch_sts_jar.py with embedded sts/GoldMod.class.

Patches:
  1. AbstractPlayer.gainGold(I)V       -> configurable gold multiplier (sts_cheat.ini)
  2. AbstractPlayer.initializeClass()  -> configurable starting_potion_slots & cards_per_turn (sts_cheat.ini)
  3. DesktopLauncher.main(String[])V   -> config.resizable = true
  4. CardCrawlGame.resize(II)V         -> aspect-ratio FitViewport.update + Settings.M_W/M_H
  5. InputHelper.updateFirst()V        -> unproject mouse coordinates through FitViewport
"""
import base64
import os

with open("build/sts/GoldMod.class", "rb") as f:
    goldmod_b64 = base64.b64encode(f.read()).decode("ascii")

PATCHER_SRC = '''#!/usr/bin/env python3
"""Patch Slay the Spire (desktop-1.0.jar) with:
  - Configurable gold multiplier, starting potion slots, and cards drawn per turn (sts_cheat.ini)
  - Resizable windowed mode (preserving aspect ratio via FitViewport + accurate mouse mapping)

Usage:
    python patch_sts_jar.py /path/to/desktop-1.0.jar
    python patch_sts_jar.py --restore /path/to/desktop-1.0.jar
"""
import base64
import os
import shutil
import struct
import sys
import zipfile

PLAYER_CLASS = "com/megacrit/cardcrawl/characters/AbstractPlayer.class"
LAUNCHER_CLASS = "com/megacrit/cardcrawl/desktop/DesktopLauncher.class"
GAME_CLASS = "com/megacrit/cardcrawl/core/CardCrawlGame.class"
INPUT_CLASS = "com/megacrit/cardcrawl/helpers/input/InputHelper.class"
HELPER_CLASS = "sts/GoldMod.class"
HELPER_BYTES = base64.b64decode("__GOLDMOD_B64__")


def parse_cp(data, off, cp_count):
    entries = [None]  # 1-indexed
    i = 1
    while i < cp_count:
        tag = data[off]
        start = off
        off += 1
        if tag == 1:  # Utf8
            ln = struct.unpack_from(">H", data, off)[0]
            off += 2 + ln
            entries.append((tag, data[start:off], data[start + 3:off].decode("utf-8", "replace")))
        elif tag in (3, 4):
            off += 4
            entries.append((tag, data[start:off], None))
        elif tag in (5, 6):
            off += 8
            entries.append((tag, data[start:off], None))
            entries.append(None)
            i += 1
        elif tag in (7, 8, 16, 19, 20):
            off += 2
            entries.append((tag, data[start:off], None))
        elif tag in (9, 10, 11, 12, 17, 18):
            off += 4
            entries.append((tag, data[start:off], None))
        elif tag == 15:
            off += 3
            entries.append((tag, data[start:off], None))
        else:
            raise ValueError(f"Unknown constant pool tag {tag} at {start}")
        i += 1
    return entries, off


def _append_methodref(new_cp_bytes, next_idx, cls_name, method_name, method_desc):
    def add_utf8(s):
        nonlocal next_idx
        b = s.encode("utf-8")
        new_cp_bytes.extend(struct.pack(">BH", 1, len(b)) + b)
        idx = next_idx
        next_idx += 1
        return idx

    def add_class(name_idx):
        nonlocal next_idx
        new_cp_bytes.extend(struct.pack(">BH", 7, name_idx))
        idx = next_idx
        next_idx += 1
        return idx

    def add_nat(name_idx, desc_idx):
        nonlocal next_idx
        new_cp_bytes.extend(struct.pack(">BHH", 12, name_idx, desc_idx))
        idx = next_idx
        next_idx += 1
        return idx

    def add_methodref(cls_idx, nat_idx):
        nonlocal next_idx
        new_cp_bytes.extend(struct.pack(">BHH", 10, cls_idx, nat_idx))
        idx = next_idx
        next_idx += 1
        return idx

    u_cls = add_utf8(cls_name)
    u_name = add_utf8(method_name)
    u_desc = add_utf8(method_desc)
    c_idx = add_class(u_cls)
    nat_idx = add_nat(u_name, u_desc)
    mref_idx = add_methodref(c_idx, nat_idx)
    return mref_idx, next_idx


def _skip_header_and_fields(cls_bytes, off):
    head_start = off
    off += 6  # access, this, super
    if_count = struct.unpack_from(">H", cls_bytes, off)[0]
    off += 2 + 2 * if_count
    fields_count = struct.unpack_from(">H", cls_bytes, off)[0]
    off += 2
    for _ in range(fields_count):
        off += 6
        ac = struct.unpack_from(">H", cls_bytes, off)[0]
        off += 2
        for _ in range(ac):
            _, alen = struct.unpack_from(">HI", cls_bytes, off)
            off += 6 + alen
    return cls_bytes[head_start:off], off


def patch_abstract_player_class(cls_bytes):
    magic, minor, major, cp_count = struct.unpack_from(">IHHH", cls_bytes, 0)
    assert magic == 0xCAFEBABE, "Not a valid Java class file"
    cp, off = parse_cp(cls_bytes, 10, cp_count)

    for e in cp:
        if e and e[0] == 1 and e[2] == "sts/GoldMod":
            return None  # Already patched

    new_cp_bytes = bytearray(cls_bytes[10:off])
    mref_scale, next_idx = _append_methodref(new_cp_bytes, cp_count, "sts/GoldMod", "scale", "(I)I")
    mref_init, next_idx = _append_methodref(
        new_cp_bytes, next_idx, "sts/GoldMod", "onInitPlayer", "(Lcom/megacrit/cardcrawl/characters/AbstractPlayer;)V"
    )

    body = bytearray()
    hdr_fields, off = _skip_header_and_fields(cls_bytes, off)
    body.extend(hdr_fields)

    methods_count = struct.unpack_from(">H", cls_bytes, off)[0]
    body.extend(cls_bytes[off:off + 2])
    off += 2
    patched_gold = False
    patched_init = False

    for _ in range(methods_count):
        m_start = off
        acc, name_idx, desc_idx, attr_count = struct.unpack_from(">HHHH", cls_bytes, off)
        m_name = cp[name_idx][2]
        m_desc = cp[desc_idx][2]
        off += 8
        is_gold = (m_name == "gainGold" and m_desc == "(I)V")
        is_init = (m_name == "initializeClass" and m_desc.endswith(")V"))
        body.extend(cls_bytes[m_start:off])

        for _ in range(attr_count):
            aname_idx, alen = struct.unpack_from(">HI", cls_bytes, off)
            aname = cp[aname_idx][2]
            adata = cls_bytes[off + 6:off + 6 + alen]
            off += 6 + alen

            if is_gold and aname == "Code":
                max_stack, max_locals, code_len = struct.unpack_from(">HHI", adata, 0)
                orig_code = adata[8:8 + code_len]
                prefix = struct.pack(">BBHB", 0x1B, 0xB8, mref_scale, 0x3C)
                delta = len(prefix)  # 5
                new_code = prefix + orig_code
                if max_stack < 2:
                    max_stack = 2

                pos = 8 + code_len
                ex_len = struct.unpack_from(">H", adata, pos)[0]
                pos += 2
                new_ex = bytearray()
                for _ in range(ex_len):
                    sp, ep, hp, ct = struct.unpack_from(">HHHH", adata, pos)
                    pos += 8
                    new_ex.extend(struct.pack(">HHHH", sp + delta, ep + delta, hp + delta, ct))

                sub_count = struct.unpack_from(">H", adata, pos)[0]
                pos += 2
                new_subs = bytearray()
                for _ in range(sub_count):
                    sname_idx, slen = struct.unpack_from(">HI", adata, pos)
                    sname = cp[sname_idx][2]
                    sdata = bytearray(adata[pos + 6:pos + 6 + slen])
                    pos += 6 + slen

                    if sname == "LineNumberTable":
                        cnt = struct.unpack_from(">H", sdata, 0)[0]
                        for k in range(cnt):
                            pc, ln = struct.unpack_from(">HH", sdata, 2 + k * 4)
                            struct.pack_into(">HH", sdata, 2 + k * 4, pc + delta if pc > 0 else 0, ln)
                    elif sname == "LocalVariableTable":
                        cnt = struct.unpack_from(">H", sdata, 0)[0]
                        for k in range(cnt):
                            sp, ln = struct.unpack_from(">HH", sdata, 2 + k * 10)
                            if sp == 0:
                                struct.pack_into(">HH", sdata, 2 + k * 10, 0, ln + delta)
                            else:
                                struct.pack_into(">HH", sdata, 2 + k * 10, sp + delta, ln)
                    elif sname == "StackMapTable":
                        cnt = struct.unpack_from(">H", sdata, 0)[0]
                        if cnt > 0:
                            ft = sdata[2]
                            if ft < 64:
                                new_ft = ft + delta
                                if new_ft < 64:
                                    sdata[2] = new_ft
                                else:
                                    sdata[2:3] = struct.pack(">BH", 251, new_ft)
                            elif 64 <= ft < 128:
                                new_off = (ft - 64) + delta
                                if new_off < 64:
                                    sdata[2] = 64 + new_off
                                else:
                                    sdata[2:3] = struct.pack(">BH", 247, new_off)
                            elif ft in (247, 248, 249, 250, 251, 252, 253, 254, 255):
                                cur_off = struct.unpack_from(">H", sdata, 3)[0]
                                struct.pack_into(">H", sdata, 3, cur_off + delta)

                    new_subs.extend(struct.pack(">HI", sname_idx, len(sdata)) + sdata)

                new_code_attr = (
                    struct.pack(">HHI", max_stack, max_locals, len(new_code))
                    + new_code
                    + struct.pack(">H", ex_len)
                    + new_ex
                    + struct.pack(">H", sub_count)
                    + new_subs
                )
                body.extend(struct.pack(">HI", aname_idx, len(new_code_attr)) + new_code_attr)
                patched_gold = True
            elif is_init and aname == "Code":
                max_stack, max_locals, code_len = struct.unpack_from(">HHI", adata, 0)
                orig_code = adata[8:8 + code_len]
                assert orig_code[-1] == 0xB1, "Expected initializeClass to end with return (0xB1)"
                suffix = struct.pack(">BBHB", 0x2A, 0xB8, mref_init, 0xB1)
                new_code = orig_code[:-1] + suffix
                delta = len(suffix) - 1  # 4
                if max_stack < 1:
                    max_stack = 1

                pos = 8 + code_len
                ex_len = struct.unpack_from(">H", adata, pos)[0]
                ex_bytes = adata[pos + 2:pos + 2 + ex_len * 8]
                pos += 2 + ex_len * 8
                sub_count = struct.unpack_from(">H", adata, pos)[0]
                pos += 2
                new_subs = bytearray()
                for _ in range(sub_count):
                    sname_idx, slen = struct.unpack_from(">HI", adata, pos)
                    sname = cp[sname_idx][2]
                    sdata = bytearray(adata[pos + 6:pos + 6 + slen])
                    pos += 6 + slen
                    if sname == "LocalVariableTable":
                        cnt = struct.unpack_from(">H", sdata, 0)[0]
                        for k in range(cnt):
                            sp, ln = struct.unpack_from(">HH", sdata, 2 + k * 10)
                            if sp + ln >= code_len:
                                struct.pack_into(">HH", sdata, 2 + k * 10, sp, ln + delta)
                    new_subs.extend(struct.pack(">HI", sname_idx, len(sdata)) + sdata)

                new_code_attr = (
                    struct.pack(">HHI", max_stack, max_locals, len(new_code))
                    + new_code
                    + struct.pack(">H", ex_len)
                    + ex_bytes
                    + struct.pack(">H", sub_count)
                    + new_subs
                )
                body.extend(struct.pack(">HI", aname_idx, len(new_code_attr)) + new_code_attr)
                patched_init = True
            else:
                body.extend(struct.pack(">HI", aname_idx, alen) + adata)

    assert patched_gold, "Could not find AbstractPlayer.gainGold(I)V"
    assert patched_init, "Could not find AbstractPlayer.initializeClass(...)V"
    body.extend(cls_bytes[off:])
    return struct.pack(">IHHH", magic, minor, major, next_idx) + bytes(new_cp_bytes) + bytes(body)


def patch_desktop_launcher_class(cls_bytes):
    """Set LwjglApplicationConfiguration.resizable = true (iconst_0 -> iconst_1)."""
    magic, minor, major, cp_count = struct.unpack_from(">IHHH", cls_bytes, 0)
    assert magic == 0xCAFEBABE
    cp, off = parse_cp(cls_bytes, 10, cp_count)

    resizable_fref = None
    for idx in range(1, len(cp)):
        e = cp[idx]
        if e and e[0] == 9:
            cls_idx, nat_idx = struct.unpack_from(">HH", e[1], 1)
            cname_idx = struct.unpack_from(">H", cp[cls_idx][1], 1)[0]
            fname_idx, fdesc_idx = struct.unpack_from(">HH", cp[nat_idx][1], 1)
            if (cp[cname_idx][2] == "com/badlogic/gdx/backends/lwjgl/LwjglApplicationConfiguration"
                    and cp[fname_idx][2] == "resizable" and cp[fdesc_idx][2] == "Z"):
                resizable_fref = idx
                break
    assert resizable_fref is not None, "Could not find LwjglApplicationConfiguration.resizable Fieldref"

    target_pat = struct.pack(">BBH", 0x03, 0xB5, resizable_fref)
    already_pat = struct.pack(">BBH", 0x04, 0xB5, resizable_fref)
    out = bytearray(cls_bytes)
    pos = out.find(target_pat, off)
    if pos == -1:
        if out.find(already_pat, off) != -1:
            return None
        raise AssertionError("Could not find `config.resizable = false` bytecode")
    out[pos] = 0x04
    return bytes(out)


def patch_card_crawl_game_class(cls_bytes):
    """Replace empty CardCrawlGame.resize(II)V with `GoldMod.onResize(w, h); return;`."""
    magic, minor, major, cp_count = struct.unpack_from(">IHHH", cls_bytes, 0)
    assert magic == 0xCAFEBABE
    cp, off = parse_cp(cls_bytes, 10, cp_count)

    for e in cp:
        if e and e[0] == 1 and e[2] == "sts/GoldMod":
            return None

    new_cp_bytes = bytearray(cls_bytes[10:off])
    mref_idx, next_idx = _append_methodref(new_cp_bytes, cp_count, "sts/GoldMod", "onResize", "(II)V")

    body = bytearray()
    hdr_fields, off = _skip_header_and_fields(cls_bytes, off)
    body.extend(hdr_fields)

    methods_count = struct.unpack_from(">H", cls_bytes, off)[0]
    body.extend(cls_bytes[off:off + 2])
    off += 2
    patched = False

    for _ in range(methods_count):
        m_start = off
        acc, name_idx, desc_idx, attr_count = struct.unpack_from(">HHHH", cls_bytes, off)
        m_name = cp[name_idx][2]
        m_desc = cp[desc_idx][2]
        off += 8
        is_target = (m_name == "resize" and m_desc == "(II)V")
        body.extend(cls_bytes[m_start:off])

        for _ in range(attr_count):
            aname_idx, alen = struct.unpack_from(">HI", cls_bytes, off)
            aname = cp[aname_idx][2]
            adata = cls_bytes[off + 6:off + 6 + alen]
            off += 6 + alen

            if is_target and aname == "Code":
                max_stack, max_locals, code_len = struct.unpack_from(">HHI", adata, 0)
                new_code = struct.pack(">BBBHB", 0x1B, 0x1C, 0xB8, mref_idx, 0xB1)
                new_code_attr = (
                    struct.pack(">HHI", max(max_stack, 2), max(max_locals, 3), len(new_code))
                    + new_code
                    + struct.pack(">HH", 0, 0)
                )
                body.extend(struct.pack(">HI", aname_idx, len(new_code_attr)) + new_code_attr)
                patched = True
            else:
                body.extend(struct.pack(">HI", aname_idx, alen) + adata)

    assert patched, "Could not find CardCrawlGame.resize(II)V"
    body.extend(cls_bytes[off:])
    return struct.pack(">IHHH", magic, minor, major, next_idx) + bytes(new_cp_bytes) + bytes(body)


def patch_input_helper_class(cls_bytes):
    """Append `GoldMod.fixMouse()` right before the final `return` of InputHelper.updateFirst()V."""
    magic, minor, major, cp_count = struct.unpack_from(">IHHH", cls_bytes, 0)
    assert magic == 0xCAFEBABE
    cp, off = parse_cp(cls_bytes, 10, cp_count)

    for e in cp:
        if e and e[0] == 1 and e[2] == "sts/GoldMod":
            return None

    new_cp_bytes = bytearray(cls_bytes[10:off])
    mref_idx, next_idx = _append_methodref(new_cp_bytes, cp_count, "sts/GoldMod", "fixMouse", "()V")

    body = bytearray()
    hdr_fields, off = _skip_header_and_fields(cls_bytes, off)
    body.extend(hdr_fields)

    methods_count = struct.unpack_from(">H", cls_bytes, off)[0]
    body.extend(cls_bytes[off:off + 2])
    off += 2
    patched = False

    for _ in range(methods_count):
        m_start = off
        acc, name_idx, desc_idx, attr_count = struct.unpack_from(">HHHH", cls_bytes, off)
        m_name = cp[name_idx][2]
        m_desc = cp[desc_idx][2]
        off += 8
        is_target = (m_name == "updateFirst" and m_desc == "()V")
        body.extend(cls_bytes[m_start:off])

        for _ in range(attr_count):
            aname_idx, alen = struct.unpack_from(">HI", cls_bytes, off)
            aname = cp[aname_idx][2]
            adata = cls_bytes[off + 6:off + 6 + alen]
            off += 6 + alen

            if is_target and aname == "Code":
                max_stack, max_locals, code_len = struct.unpack_from(">HHI", adata, 0)
                orig_code = adata[8:8 + code_len]
                assert orig_code[-1] == 0xB1, "Expected updateFirst() to end with return (0xB1)"
                new_code = orig_code[:-1] + struct.pack(">BHB", 0xB8, mref_idx, 0xB1)
                tail = adata[8 + code_len:]
                new_code_attr = struct.pack(">HHI", max_stack, max_locals, len(new_code)) + new_code + tail
                body.extend(struct.pack(">HI", aname_idx, len(new_code_attr)) + new_code_attr)
                patched = True
            else:
                body.extend(struct.pack(">HI", aname_idx, alen) + adata)

    assert patched, "Could not find InputHelper.updateFirst()V"
    body.extend(cls_bytes[off:])
    return struct.pack(">IHHH", magic, minor, major, next_idx) + bytes(new_cp_bytes) + bytes(body)


def main():
    args = sys.argv[1:]
    restore = False
    if args and args[0] == "--restore":
        restore = True
        args = args[1:]
    if len(args) != 1:
        print("Usage: python patch_sts_jar.py [--restore] <path_to_desktop-1.0.jar>")
        sys.exit(1)

    jar_path = args[0]
    bak_path = jar_path + ".bak"

    if restore:
        if not os.path.exists(bak_path):
            print(f"Backup not found: {bak_path}")
            sys.exit(1)
        shutil.copy2(bak_path, jar_path)
        print(f"Restored {jar_path} from {bak_path}")
        return

    with zipfile.ZipFile(jar_path, "r") as zin:
        p_cls = patch_abstract_player_class(zin.read(PLAYER_CLASS))
        l_cls = patch_desktop_launcher_class(zin.read(LAUNCHER_CLASS))
        g_cls = patch_card_crawl_game_class(zin.read(GAME_CLASS))
        i_cls = patch_input_helper_class(zin.read(INPUT_CLASS))

    overrides = {}
    if p_cls is not None: overrides[PLAYER_CLASS] = p_cls
    if l_cls is not None: overrides[LAUNCHER_CLASS] = l_cls
    if g_cls is not None: overrides[GAME_CLASS] = g_cls
    if i_cls is not None: overrides[INPUT_CLASS] = i_cls

    if not overrides:
        print("All patches already applied! Updating sts/GoldMod.class only.")

    if not os.path.exists(bak_path):
        shutil.copy2(jar_path, bak_path)
        print(f"Created backup: {bak_path}")

    tmp_path = jar_path + ".tmp"
    with zipfile.ZipFile(jar_path, "r") as zin, zipfile.ZipFile(tmp_path, "w") as zout:
        for item in zin.infolist():
            if item.filename in overrides:
                zout.writestr(item, overrides[item.filename])
            elif item.filename == HELPER_CLASS:
                continue
            else:
                zout.writestr(item, zin.read(item.filename))
        zout.writestr(HELPER_CLASS, HELPER_BYTES, compress_type=zipfile.ZIP_DEFLATED)

    os.replace(tmp_path, jar_path)
    print(f"Successfully patched {jar_path} ({len(overrides)} class(es) + {HELPER_CLASS})!")
    print("Put sts_cheat.ini next to the jar to configure gold_multiplier, starting_potion_slots, and cards_per_turn.")


if __name__ == "__main__":
    main()
'''.replace("__GOLDMOD_B64__", goldmod_b64)

os.makedirs("release", exist_ok=True)
with open("release/patch_sts_jar.py", "w", encoding="utf-8", newline="\n") as f:
    f.write(PATCHER_SRC)
os.chmod("release/patch_sts_jar.py", 0o755)
print(f"Wrote release/patch_sts_jar.py (GoldMod.class = {len(base64.b64decode(goldmod_b64))} bytes)")
