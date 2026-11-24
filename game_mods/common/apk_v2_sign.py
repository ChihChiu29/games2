#!/usr/bin/env python3
"""Reusable 4KB-aligned APK repacker + APK Signature Scheme v2 signer (Android 7.0–16+).

Uses `cryptography` (RSA-2048 + SHA-256, alg ID 0x0103) and self-verifies the v2 block.
"""
import datetime
import hashlib
import os
import struct
import zipfile
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import padding, rsa
from cryptography.x509.oid import NameOID

CHUNK_SIZE = 1024 * 1024
SIG_ALG_RSA_PKCS1_V1_5_SHA256 = 0x0103
APK_SIG_SCHEME_V2_BLOCK_ID = 0x7109871A
APK_SIG_BLOCK_MAGIC = b"APK Sig Block 42"


def get_or_create_key(key_dir):
    os.makedirs(key_dir, exist_ok=True)
    key_path = os.path.join(key_dir, "mod_debug_key.pem")
    cert_path = os.path.join(key_dir, "mod_debug_cert.der")
    if os.path.exists(key_path) and os.path.exists(cert_path):
        with open(key_path, "rb") as f:
            priv = serialization.load_pem_private_key(f.read(), password=None)
        with open(cert_path, "rb") as f:
            cert_der = f.read()
        return priv, cert_der

    priv = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    subject = issuer = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "GameMods Debug")])
    now = datetime.datetime.now(datetime.timezone.utc)
    cert = (
        x509.CertificateBuilder()
        .subject_name(subject)
        .issuer_name(issuer)
        .public_key(priv.public_key())
        .serial_number(x509.random_serial_number())
        .not_valid_before(now - datetime.timedelta(days=1))
        .not_valid_after(now + datetime.timedelta(days=3650))
        .sign(priv, hashes.SHA256())
    )
    cert_der = cert.public_bytes(serialization.Encoding.DER)
    with open(key_path, "wb") as f:
        f.write(priv.private_bytes(
            serialization.Encoding.PEM,
            serialization.PrivateFormat.PKCS8,
            serialization.NoEncryption(),
        ))
    with open(cert_path, "wb") as f:
        f.write(cert_der)
    return priv, cert_der


def _lp(b: bytes) -> bytes:
    return struct.pack("<I", len(b)) + b


def _hash_stream_chunks(f, start, length, chunk_digests):
    f.seek(start)
    rem = length
    while rem > 0:
        n = min(CHUNK_SIZE, rem)
        buf = f.read(n)
        if not buf:
            break
        h = hashlib.sha256()
        h.update(b"\xa5")
        h.update(struct.pack("<I", len(buf)))
        h.update(buf)
        chunk_digests.append(h.digest())
        rem -= len(buf)


def _hash_bytes_chunks(data: bytes, chunk_digests):
    off = 0
    n = len(data)
    while off < n:
        chunk = data[off:off + CHUNK_SIZE]
        h = hashlib.sha256()
        h.update(b"\xa5")
        h.update(struct.pack("<I", len(chunk)))
        h.update(chunk)
        chunk_digests.append(h.digest())
        off += len(chunk)


def find_eocd(f):
    f.seek(0, os.SEEK_END)
    file_size = f.tell()
    tail_len = min(file_size, 65557)
    f.seek(file_size - tail_len)
    tail = f.read(tail_len)
    idx = tail.rfind(b"PK\x05\x06")
    assert idx != -1, "EOCD not found in APK"
    eocd_off = file_size - tail_len + idx
    eocd = bytearray(tail[idx:])
    cd_size, cd_off = struct.unpack_from("<II", eocd, 12)
    return eocd_off, cd_off, cd_size, eocd


def sign_apk_v2(unsigned_apk, signed_apk, key_dir):
    priv, cert_der = get_or_create_key(key_dir)
    pub_der = priv.public_key().public_bytes(
        serialization.Encoding.DER,
        serialization.PublicFormat.SubjectPublicKeyInfo,
    )

    with open(unsigned_apk, "rb") as fin:
        eocd_off, cd_off, cd_size, eocd = find_eocd(fin)
        assert cd_off + cd_size == eocd_off, "Unexpected gap between CD and EOCD"

        # Compute APK Signature Scheme v2 digest over Section 1, Section 3 (CD), Section 4 (EOCD with cd_off = section1_end)
        chunk_digests = []
        _hash_stream_chunks(fin, 0, cd_off, chunk_digests)
        _hash_stream_chunks(fin, cd_off, cd_size, chunk_digests)
        # EOCD already has cd_off pointing to end of Section 1 (which will be start of Signing Block)
        _hash_bytes_chunks(bytes(eocd), chunk_digests)

        top = hashlib.sha256()
        top.update(b"\x5a")
        top.update(struct.pack("<I", len(chunk_digests)))
        for d in chunk_digests:
            top.update(d)
        top_digest = top.digest()

        digests_seq = _lp(struct.pack("<I", SIG_ALG_RSA_PKCS1_V1_5_SHA256) + _lp(top_digest))
        certs_seq = _lp(cert_der)
        attrs_seq = b""
        signed_data = _lp(digests_seq) + _lp(certs_seq) + _lp(attrs_seq)

        sig = priv.sign(signed_data, padding.PKCS1v15(), hashes.SHA256())
        sigs_seq = _lp(struct.pack("<I", SIG_ALG_RSA_PKCS1_V1_5_SHA256) + _lp(sig))

        signer = _lp(signed_data) + _lp(sigs_seq) + _lp(pub_der)
        v2_val = _lp(_lp(signer))

        pair = struct.pack("<QI", 4 + len(v2_val), APK_SIG_SCHEME_V2_BLOCK_ID) + v2_val
        # Size of block (excluding leading 8-byte size field)
        block_size = len(pair) + 8 + 16
        sig_block = struct.pack("<Q", block_size) + pair + struct.pack("<Q", block_size) + APK_SIG_BLOCK_MAGIC

        # Update EOCD cd_off
        new_cd_off = cd_off + len(sig_block)
        struct.pack_into("<I", eocd, 16, new_cd_off)

        # Write final signed APK
        with open(signed_apk, "wb") as fout:
            fin.seek(0)
            rem = cd_off
            while rem > 0:
                buf = fin.read(min(4 * 1024 * 1024, rem))
                fout.write(buf)
                rem -= len(buf)
            fout.write(sig_block)
            fin.seek(cd_off)
            rem = cd_size
            while rem > 0:
                buf = fin.read(min(4 * 1024 * 1024, rem))
                fout.write(buf)
                rem -= len(buf)
            fout.write(eocd)

    # Self-verify signature
    priv.public_key().verify(sig, signed_data, padding.PKCS1v15(), hashes.SHA256())


def repack_apk_aligned(in_apk, out_unsigned_apk, overrides: dict):
    """Repack APK stripping old META-INF signatures, applying `overrides` (filename -> bytes),
    and preserving ZIP_STORED + 4096-byte alignment for .so / 4-byte alignment for other stored entries."""
    with zipfile.ZipFile(in_apk, "r") as zin, zipfile.ZipFile(out_unsigned_apk, "w") as zout:
        for item in zin.infolist():
            fn = item.filename
            # Strip old v1 signature files
            if fn.startswith("META-INF/") and (
                fn == "META-INF/MANIFEST.MF" or fn.endswith((".SF", ".RSA", ".DSA", ".EC"))
            ):
                continue

            data = overrides[fn] if fn in overrides else zin.read(fn)

            zi = zipfile.ZipInfo(fn, date_time=item.date_time)
            zi.compress_type = item.compress_type
            zi.external_attr = item.external_attr
            zi.flag_bits = item.flag_bits & ~0x08  # clear data descriptor bit

            if zi.compress_type == zipfile.ZIP_STORED:
                align = 4096 if fn.endswith(".so") else 4
                # Local file header size = 30 + len(filename) + len(extra)
                cur_pos = zout.fp.tell()
                data_start = cur_pos + 30 + len(fn.encode("utf-8"))
                pad = (align - (data_start % align)) % align
                if pad:
                    zi.extra = b"\x00" * pad

            zout.writestr(zi, data, compress_type=zi.compress_type)
