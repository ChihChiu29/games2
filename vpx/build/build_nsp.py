#!/usr/bin/env python3
"""
Packages VPNX (Switch port of Visual Pinball) and all bundled tables into a
standalone Nintendo Switch .nsp package (Program NCA + Control NCA + Meta NCA).
"""

import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

IVFC_MAX_LEVEL = 6
IVFC_HASH_BLOCK_SIZE = 0x4000
PFS0_EXEFS_HASH_BLOCK_SIZE = 0x10000
PFS0_META_HASH_BLOCK_SIZE = 0x1000
PFS0_PADDING_SIZE = 0x200

def derive_retail_header_key() -> bytes:
    # Derive retail NCA header_key via standard Switch key schedule:
    # splCryptoGenerateAesKek(HEADER_KEK_SRC, 0, 0) + splCryptoGenerateAesKey(HEADER_KEY_SRC)
    master_key_00 = bytes.fromhex("c2caaff089b9aed55694876055271c7d")
    aes_kek_gen_src = bytes.fromhex("4d870986c45d20722fba1053da92e8a9")
    aes_key_gen_src = bytes.fromhex("89615ee05c38b6805fe58f3da24f7aa8")
    header_kek_src = bytes.fromhex("1f12913a4acbf00d4cde3af6d523882a")
    header_key_src = bytes.fromhex(
        "5a3ed84fdec0d82631f7e25d197bf5d01c9b7bfaf628183d71f64d73f150b9d2"
    )

    def ecb_dec(key: bytes, data: bytes) -> bytes:
        c = Cipher(algorithms.AES(key), modes.ECB()).decryptor()
        return c.update(data) + c.finalize()

    kek_seed = ecb_dec(master_key_00, aes_kek_gen_src)
    kek_part = ecb_dec(kek_seed, header_kek_src)
    header_kek = ecb_dec(kek_part, aes_key_gen_src)
    return ecb_dec(header_kek, header_key_src)


def aes_xts_nintendo_encrypt_header(header_bytes: bytes, key: bytes) -> bytes:
    assert len(header_bytes) == 0xC00
    assert len(key) == 32
    data_ecb = Cipher(algorithms.AES(key[:16]), modes.ECB()).encryptor()
    tweak_ecb = Cipher(algorithms.AES(key[16:]), modes.ECB()).encryptor()
    out = bytearray()
    for sector in range(6):
        # Nintendo XTS tweak: 128-bit big-endian sector index
        tweak_in = sector.to_bytes(16, "big")
        tweak_enc = tweak_ecb.update(tweak_in)
        t_val = int.from_bytes(tweak_enc, "little")
        sec_data = header_bytes[sector * 0x200 : (sector + 1) * 0x200]
        for blk_off in range(0, 0x200, 16):
            blk = int.from_bytes(sec_data[blk_off : blk_off + 16], "little")
            xored = (blk ^ t_val).to_bytes(16, "little")
            enc = int.from_bytes(data_ecb.update(xored), "little")
            out.extend((enc ^ t_val).to_bytes(16, "little"))
            carry = (t_val >> 127) & 1
            t_val = ((t_val << 1) & ((1 << 128) - 1)) ^ (0x87 if carry else 0)
    return bytes(out)


def make_title_id(seed: str = "vpnx-forwarder:launcher") -> int:
    h = hashlib.sha256(seed.encode("utf-8")).digest()
    val = struct.unpack("<Q", h[:8])[0]
    return 0x0500000000000000 | (val & 0x00FFFFFFFFFFF000)


def npdm_patch_kc(npdm: bytearray, off: int, size: int, bitmask: int, value: int) -> bool:
    if (size & 3) != 0 or off > len(npdm) or size > len(npdm) - off:
        return False
    pattern = (1 << bitmask) - 1
    mask = (1 << bitmask) | pattern
    for i in range(0, size, 4):
        cup = struct.unpack_from("<I", npdm, off + i)[0]
        if (cup & mask) == pattern:
            cup = value | pattern
            struct.pack_into("<I", npdm, off + i, cup)
            return True
    return False


def patch_npdm(npdm_bytes: bytes, tid: int) -> bytes:
    npdm = bytearray(npdm_bytes)
    aci0_off, aci0_sz, acid_off, acid_sz = struct.unpack_from("<4I", npdm, 0x70)
    # Patch program_id in ACI0 (+0x10) and program_id_min/max in ACID (+0x210, +0x218)
    struct.pack_into("<Q", npdm, aci0_off + 0x10, tid)
    struct.pack_into("<QQ", npdm, acid_off + 0x210, tid, tid)
    # Patch Atmosphere 1.8+ bit 19 in both KAC sections
    aci_kac_off, aci_kac_sz = struct.unpack_from("<II", npdm, aci0_off + 0x30)
    acid_kac_off, acid_kac_sz = struct.unpack_from("<II", npdm, acid_off + 0x230)
    npdm_patch_kc(npdm, aci0_off + aci_kac_off, aci_kac_sz, 16, 1 << 19)
    npdm_patch_kc(npdm, acid_off + acid_kac_off, acid_kac_sz, 16, 1 << 19)
    return bytes(npdm)


def patch_nacp(nacp_bytes: bytes, name: str, author: str, tid: int) -> bytes:
    nacp = bytearray(nacp_bytes)
    assert len(nacp) == 0x4000
    name_b = name.encode("utf-8")[:0x1FF]
    author_b = author.encode("utf-8")[:0xFF]
    for i in range(16):
        entry_off = i * 0x300
        nacp[entry_off : entry_off + 0x200] = name_b.ljust(0x200, b"\x00")
        nacp[entry_off + 0x200 : entry_off + 0x300] = author_b.ljust(0x100, b"\x00")
    nacp[0x3024] = 0x00  # startup_user_account
    nacp[0x3025] = 0x00  # user_account_switch_lock
    nacp[0x3026] = 0x01  # add_on_content_registration_type
    nacp[0x3034] = 0x00  # screenshot
    nacp[0x3035] = 0x02  # video_capture
    nacp[0x30F0] = 0x02  # logo_type
    nacp[0x30F1] = 0x00  # logo_handling
    nacp[0x30F4] = 0x00  # data_loss_confirmation
    struct.pack_into("<Q", nacp, 0x3038, tid)  # presence_group_id
    struct.pack_into("<Q", nacp, 0x3078, tid)  # save_data_owner_id
    struct.pack_into("<Q", nacp, 0x30A8, tid ^ 0x1000)  # add_on_content_base_id
    struct.pack_into("<Q", nacp, 0x30B0, tid)  # pseudo_device_id_seed
    for i in range(8):
        struct.pack_into("<Q", nacp, 0x30B8 + i * 8, tid)  # local_communication_id
    return bytes(nacp)


def build_pfs0_hash_table(pfs0_data: bytes, block_size: int) -> bytes:
    out = bytearray()
    for i in range(0, len(pfs0_data), block_size):
        chunk = pfs0_data[i : i + block_size]
        out.extend(hashlib.sha256(chunk).digest())
    return bytes(out)


def pad_bytes(size: int, block: int) -> bytes:
    # Matches write_padding in switch/forwarder.cpp
    rem = size % block
    pad_len = (block - rem) if rem != 0 else 0
    return b"\x00" * pad_len


def build_pfs0_section(pfs0_data: bytes, block_size: int) -> tuple[bytes, bytes]:
    hash_table = build_pfs0_hash_table(pfs0_data, block_size)
    master_hash = hashlib.sha256(hash_table).digest()
    rem = len(hash_table) % PFS0_PADDING_SIZE
    pad1 = (PFS0_PADDING_SIZE - rem) if rem != 0 else PFS0_PADDING_SIZE
    body = bytearray()
    body.extend(hash_table)
    body.extend(b"\x00" * pad1)
    pfs0_off = len(body)
    body.extend(pfs0_data)
    rem2 = len(body) % 0x200
    pad2 = (0x200 - rem2) if rem2 != 0 else 0
    body.extend(b"\x00" * pad2)

    fs_header = bytearray(0x200)
    struct.pack_into("<BBBBBB", fs_header, 0, 2, 0, 1, 2, 1, 0)  # version=2, fs_type=PFS0(1), hash_type=2, enc=None(1)
    fs_header[0x08:0x28] = master_hash
    struct.pack_into("<IIQQQQ", fs_header, 0x28, block_size, 2, 0, len(hash_table), pfs0_off, len(pfs0_data))
    return bytes(body), bytes(fs_header)


def build_romfs_section_from_file(romfs_path: Path, body_out) -> tuple[int, bytes]:
    """
    Streams IVFC levels 0..5 of romfs_path into body_out and returns (section_byte_length, fs_header_bytes).
    """
    raw_size = romfs_path.stat().st_size
    rem = raw_size % IVFC_HASH_BLOCK_SIZE
    lvl5_pad = (IVFC_HASH_BLOCK_SIZE - rem) if rem != 0 else 0

    # Compute level 4 hashes by streaming level 5 (romfs_path + lvl5_pad) in 16 KiB blocks
    lvl4 = bytearray()
    with open(romfs_path, "rb") as f:
        while True:
            chunk = f.read(IVFC_HASH_BLOCK_SIZE)
            if not chunk:
                break
            if len(chunk) < IVFC_HASH_BLOCK_SIZE:
                chunk = chunk + (b"\x00" * (IVFC_HASH_BLOCK_SIZE - len(chunk)))
            lvl4.extend(hashlib.sha256(chunk).digest())
    rem4 = len(lvl4) % IVFC_HASH_BLOCK_SIZE
    if rem4 != 0:
        lvl4.extend(b"\x00" * (IVFC_HASH_BLOCK_SIZE - rem4))

    levels_data = [None] * 5
    levels_data[4] = bytes(lvl4)
    for b in range(3, -1, -1):
        src = levels_data[b + 1]
        cur = bytearray()
        for i in range(0, len(src), IVFC_HASH_BLOCK_SIZE):
            cur.extend(hashlib.sha256(src[i : i + IVFC_HASH_BLOCK_SIZE]).digest())
        rem_b = len(cur) % IVFC_HASH_BLOCK_SIZE
        if rem_b != 0 or len(cur) == 0:
            cur.extend(b"\x00" * (IVFC_HASH_BLOCK_SIZE - rem_b))
        levels_data[b] = bytes(cur)

    hash_data_sizes = [len(levels_data[i]) for i in range(5)] + [raw_size]
    logical_offsets = [0] * 6
    for i in range(1, 6):
        logical_offsets[i] = logical_offsets[i - 1] + hash_data_sizes[i - 1]

    master_hash = hashlib.sha256(levels_data[0]).digest()

    start_pos = body_out.tell()
    for b in range(5):
        body_out.write(levels_data[b])
    with open(romfs_path, "rb") as f:
        shutil.copyfileobj(f, body_out, length=4 * 1024 * 1024)
    if lvl5_pad:
        body_out.write(b"\x00" * lvl5_pad)
    sec_len = body_out.tell() - start_pos
    rem_sec = sec_len % 0x200
    if rem_sec != 0:
        body_out.write(b"\x00" * (0x200 - rem_sec))
        sec_len += 0x200 - rem_sec

    fs_header = bytearray(0x200)
    struct.pack_into("<BBBBBB", fs_header, 0, 2, 0, 0, 3, 1, 0)  # version=2, fs_type=RomFS(0), hash_type=3, enc=None(1)
    # IntegrityMetaInfo at +0x08
    struct.pack_into("<IIII", fs_header, 0x08, 0x43465649, 0x20000, 0x20, 7)
    for i in range(6):
        struct.pack_into(
            "<QQII",
            fs_header,
            0x18 + i * 0x18,
            logical_offsets[i],
            hash_data_sizes[i],
            0x0E,
            0,
        )
    fs_header[0xC8:0xE8] = master_hash
    return sec_len, bytes(fs_header)


def finalize_nca_file(nca_path: Path, tid: int, content_type: int, sections: list[tuple[int, int, bytes]], header_key: bytes) -> tuple[bytes, int]:
    total_size = nca_path.stat().st_size
    header = bytearray(0xC00)
    struct.pack_into("<IBBBBQQII", header, 0x200, 0x3341434E, 0, content_type, 0, 0, total_size, tid, 0, 0x000C1100)
    for idx, (start_off, end_off, fs_hdr) in enumerate(sections):
        struct.pack_into("<IIBBBB", header, 0x240 + idx * 0x10, start_off // 0x200, end_off // 0x200, 1, 0, 0, 0)
        header[0x280 + idx * 0x20 : 0x280 + (idx + 1) * 0x20] = hashlib.sha256(fs_hdr).digest()
        header[0x400 + idx * 0x200 : 0x400 + (idx + 1) * 0x200] = fs_hdr

    enc_header = aes_xts_nintendo_encrypt_header(bytes(header), header_key)
    with open(nca_path, "r+b") as f:
        f.seek(0)
        f.write(enc_header)

    h = hashlib.sha256()
    with open(nca_path, "rb") as f:
        while True:
            chunk = f.read(4 * 1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.digest(), total_size


def main():
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <devkitpro_dir> <vpnx_dir> <romfs_staging_dir> <out_nsp_path>", file=sys.stderr)
        sys.exit(1)

    dkp_dir = Path(sys.argv[1]).resolve()
    vpnx_dir = Path(sys.argv[2]).resolve()
    romfs_dir = Path(sys.argv[3]).resolve()
    out_nsp = Path(sys.argv[4]).resolve()

    build_romfs_bin = dkp_dir / "tools" / "bin" / "build_romfs"
    build_pfs0_bin = dkp_dir / "tools" / "bin" / "build_pfs0"

    tid = make_title_id("vpnx-forwarder:launcher")
    header_key = derive_retail_header_key()
    print(f"[NSP] Title ID: 0x{tid:016X}")

    fwd_dir = vpnx_dir / "romfs" / "forwarder"
    nso_bytes = (fwd_dir / "hbl.nso").read_bytes()
    npdm_bytes = patch_npdm((fwd_dir / "hbl.npdm").read_bytes(), tid)
    nacp_bytes = patch_nacp((fwd_dir / "default.nacp").read_bytes(), "VPNX", "RumaTeam", tid)
    icon_bytes = (vpnx_dir / "docs" / "brand" / "icon.jpg").read_bytes()

    with tempfile.TemporaryDirectory(prefix="vpnx_nsp_") as tmp:
        tmp_path = Path(tmp)
        nsp_dir = tmp_path / "nsp_files"
        nsp_dir.mkdir()

        # 1. Build Program NCA (Section 0 = ExeFS PFS0, Section 1 = RomFS IVFC)
        print("[NSP] Building Program NCA (ExeFS + RomFS with bundled tables)...")
        exefs_dir = tmp_path / "exefs"
        exefs_dir.mkdir()
        (exefs_dir / "main").write_bytes(nso_bytes)
        (exefs_dir / "main.npdm").write_bytes(npdm_bytes)
        exefs_pfs0_path = tmp_path / "exefs.pfs0"
        subprocess.run([str(build_pfs0_bin), str(exefs_dir), str(exefs_pfs0_path)], check=True)
        sec0_body, sec0_hdr = build_pfs0_section(exefs_pfs0_path.read_bytes(), PFS0_EXEFS_HASH_BLOCK_SIZE)

        prog_romfs_path = tmp_path / "program.romfs"
        subprocess.run([str(build_romfs_bin), str(romfs_dir), str(prog_romfs_path)], check=True)

        prog_nca_tmp = tmp_path / "program.nca"
        with open(prog_nca_tmp, "wb") as f:
            f.write(b"\x00" * 0xC00)
            sec0_start = f.tell()
            f.write(sec0_body)
            sec0_end = f.tell()
            sec1_start = sec0_end
            sec1_len, sec1_hdr = build_romfs_section_from_file(prog_romfs_path, f)
            sec1_end = sec1_start + sec1_len

        prog_hash, prog_size = finalize_nca_file(
            prog_nca_tmp,
            tid,
            0,  # ContentType_Program
            [(sec0_start, sec0_end, sec0_hdr), (sec1_start, sec1_end, sec1_hdr)],
            header_key,
        )
        prog_nca_name = f"{prog_hash[:16].hex()}.nca"
        shutil.move(str(prog_nca_tmp), str(nsp_dir / prog_nca_name))
        print(f"  -> Program NCA: {prog_nca_name} ({prog_size / (1024*1024):.2f} MiB)")

        # 2. Build Control NCA (Section 0 = RomFS with control.nacp & icon_AmericanEnglish.dat)
        print("[NSP] Building Control NCA...")
        ctrl_romfs_dir = tmp_path / "ctrl_romfs"
        ctrl_romfs_dir.mkdir()
        (ctrl_romfs_dir / "control.nacp").write_bytes(nacp_bytes)
        (ctrl_romfs_dir / "icon_AmericanEnglish.dat").write_bytes(icon_bytes)
        ctrl_romfs_path = tmp_path / "control.romfs"
        subprocess.run([str(build_romfs_bin), str(ctrl_romfs_dir), str(ctrl_romfs_path)], check=True)

        ctrl_nca_tmp = tmp_path / "control.nca"
        with open(ctrl_nca_tmp, "wb") as f:
            f.write(b"\x00" * 0xC00)
            c0_start = f.tell()
            c0_len, c0_hdr = build_romfs_section_from_file(ctrl_romfs_path, f)
            c0_end = c0_start + c0_len

        ctrl_hash, ctrl_size = finalize_nca_file(
            ctrl_nca_tmp,
            tid,
            2,  # ContentType_Control
            [(c0_start, c0_end, c0_hdr)],
            header_key,
        )
        ctrl_nca_name = f"{ctrl_hash[:16].hex()}.nca"
        shutil.move(str(ctrl_nca_tmp), str(nsp_dir / ctrl_nca_name))
        print(f"  -> Control NCA: {ctrl_nca_name} ({ctrl_size} bytes)")

        # 3. Build Meta NCA (Section 0 = PFS0 with Application_<tid>.cnmt)
        print("[NSP] Building Meta NCA...")
        cnmt = bytearray()
        # CnmtHeader (0x20 bytes)
        cnmt.extend(
            struct.pack(
                "<QIBBHHHBBBBI4s",
                tid,
                0,     # title_version
                0x80,  # NcmContentMetaType_Application
                0,     # _0x0D
                0x10,  # extended_header_size
                2,     # content_count (Program + Control)
                0,     # content_meta_count
                0,     # attributes
                0,     # storage_id
                0,     # install_type
                0,     # _0x17
                0,     # required_download_system_version
                b"\x00" * 4,
            )
        )
        # NcmApplicationMetaExtendedHeader (0x10 bytes)
        cnmt.extend(struct.pack("<QII", tid | 0x800, 0, 0))
        # NcmPackagedContentInfo entries (2 * 0x38 bytes)
        for nca_hash, nca_size, ncm_type in [
            (prog_hash, prog_size, 1),  # NcmContentType_Program = 1
            (ctrl_hash, ctrl_size, 3),  # NcmContentType_Control = 3
        ]:
            size_low = nca_size & 0xFFFFFFFF
            size_high = (nca_size >> 32) & 0xFF
            cnmt.extend(nca_hash)        # 0x20 bytes hash
            cnmt.extend(nca_hash[:16])   # 0x10 bytes content_id
            cnmt.extend(struct.pack("<IBBBB", size_low, size_high, 0, ncm_type, 0))
        # Digest (0x20 bytes)
        cnmt.extend(b"\x00" * 0x20)

        cnmt_dir = tmp_path / "cnmt_pfs0"
        cnmt_dir.mkdir()
        (cnmt_dir / f"Application_{tid:016x}.cnmt").write_bytes(bytes(cnmt))
        cnmt_pfs0_path = tmp_path / "cnmt.pfs0"
        subprocess.run([str(build_pfs0_bin), str(cnmt_dir), str(cnmt_pfs0_path)], check=True)
        m0_body, m0_hdr = build_pfs0_section(cnmt_pfs0_path.read_bytes(), PFS0_META_HASH_BLOCK_SIZE)

        meta_nca_tmp = tmp_path / "meta.nca"
        with open(meta_nca_tmp, "wb") as f:
            f.write(b"\x00" * 0xC00)
            m0_start = f.tell()
            f.write(m0_body)
            m0_end = f.tell()

        meta_hash, meta_size = finalize_nca_file(
            meta_nca_tmp,
            tid,
            1,  # ContentType_Meta
            [(m0_start, m0_end, m0_hdr)],
            header_key,
        )
        meta_nca_name = f"{meta_hash[:16].hex()}.cnmt.nca"
        shutil.move(str(meta_nca_tmp), str(nsp_dir / meta_nca_name))
        print(f"  -> Meta NCA:    {meta_nca_name} ({meta_size} bytes)")

        # 4. Pack outer .nsp (PFS0 of the 3 NCAs)
        out_nsp.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(build_pfs0_bin), str(nsp_dir), str(out_nsp)], check=True)
        print(f"[NSP] Successfully created {out_nsp} ({out_nsp.stat().st_size / (1024*1024):.2f} MiB)")


if __name__ == "__main__":
    main()
