#!/usr/bin/env python3
"""Generate self-contained release/patch_sts_jar.py with embedded sts/GoldMod.class."""
import base64
import os

with open("build/sts/GoldMod.class", "rb") as f:
    goldmod_b64 = base64.b64encode(f.read()).decode("ascii")

PATCHER_SRC = '''#!/usr/bin/env python3
"""Patch Slay the Spire (desktop-1.0.jar) with configurable gold multiplier,
starting potion slots, and cards drawn per turn (configured via sts_gold.ini).

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

TARGET_CLASS = "com/megacrit/cardcrawl/characters/AbstractPlayer.class"
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


def patch_abstract_player_class(cls_bytes):
    magic, minor, major, cp_count = struct.unpack_from(">IHHH", cls_bytes, 0)
    assert magic == 0xCAFEBABE, "Not a valid Java class file"
    cp, off = parse_cp(cls_bytes, 10, cp_count)

    for e in cp:
        if e and e[0] == 1 and e[2] == "sts/GoldMod":
            return None  # Already patched

    new_cp_bytes = bytearray(cls_bytes[10:off])
    next_idx = cp_count

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

    u_cls = add_utf8("sts/GoldMod")
    c_idx = add_class(u_cls)

    # sts/GoldMod.scale:(I)I
    u_scale = add_utf8("scale")
    u_scale_desc = add_utf8("(I)I")
    mref_scale = add_methodref(c_idx, add_nat(u_scale, u_scale_desc))

    # sts/GoldMod.onInitPlayer:(Lcom/megacrit/cardcrawl/characters/AbstractPlayer;)V
    u_init = add_utf8("onInitPlayer")
    u_init_desc = add_utf8("(Lcom/megacrit/cardcrawl/characters/AbstractPlayer;)V")
    mref_init = add_methodref(c_idx, add_nat(u_init, u_init_desc))

    body = bytearray()
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
    body.extend(cls_bytes[head_start:off])

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
                # Append before final return: aload_0 (0x2A); invokestatic mref_init (0xB8 hi lo); return (0xB1)
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
        orig_cls = zin.read(TARGET_CLASS)

    new_cls = patch_abstract_player_class(orig_cls)
    if new_cls is None:
        print("Already patched!")
        return

    if not os.path.exists(bak_path):
        shutil.copy2(jar_path, bak_path)
        print(f"Created backup: {bak_path}")

    tmp_path = jar_path + ".tmp"
    with zipfile.ZipFile(jar_path, "r") as zin, zipfile.ZipFile(tmp_path, "w") as zout:
        for item in zin.infolist():
            if item.filename == TARGET_CLASS:
                zout.writestr(item, new_cls)
            elif item.filename == HELPER_CLASS:
                continue
            else:
                zout.writestr(item, zin.read(item.filename))
        zout.writestr(HELPER_CLASS, HELPER_BYTES, compress_type=zipfile.ZIP_DEFLATED)

    os.replace(tmp_path, jar_path)
    print(f"Successfully patched {jar_path}!")
    print("Put sts_gold.ini next to desktop-1.0.jar to configure gold_multiplier, starting_potion_slots, and cards_per_turn.")


if __name__ == "__main__":
    main()
'''.replace("__GOLDMOD_B64__", goldmod_b64)

os.makedirs("release", exist_ok=True)
with open("release/patch_sts_jar.py", "w", encoding="utf-8", newline="\n") as f:
    f.write(PATCHER_SRC)
os.chmod("release/patch_sts_jar.py", 0o755)
print(f"Wrote release/patch_sts_jar.py (GoldMod.class = {len(base64.b64decode(goldmod_b64))} bytes)")
