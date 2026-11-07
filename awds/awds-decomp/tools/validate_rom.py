#!/usr/bin/env python3
"""Validate the local AWRP00 ROM before any target-derived processing."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

EXPECTED = {
    "size": 33_554_432,
    "title": "ADVANCEWARSP",
    "game_code": "AWRP",
    "revision": 0,
    "md5": "e6105bc672701a516b730c31754f7859",
    "sha1": "03b63875930bd8fdea50902d7d2e0a4614ac36eb",
    "sha256": "a16d29f4a41748999a573dc24dba1119512ae6fa558645147b42dab5f51ad50b",
}


def digest_file(path: Path) -> dict[str, str]:
    hashes = {name: hashlib.new(name) for name in ("md5", "sha1", "sha256")}
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            for digest in hashes.values():
                digest.update(chunk)
    return {name: digest.hexdigest() for name, digest in hashes.items()}


def validate(path: Path) -> dict[str, object]:
    if not path.is_file():
        raise ValueError(f"ROM path is not a regular file: {path}")

    size = path.stat().st_size
    if size != EXPECTED["size"]:
        raise ValueError(f"unexpected ROM size: {size} (expected {EXPECTED['size']})")

    with path.open("rb") as stream:
        header = stream.read(0x200)
    if len(header) < 0x200:
        raise ValueError("ROM header is truncated")

    title = header[0:12].rstrip(b"\0").decode("ascii", errors="replace")
    game_code = header[0x0C:0x10].decode("ascii", errors="replace")
    revision = header[0x1E]
    fields = {
        "size": size,
        "title": title,
        "game_code": game_code,
        "revision": revision,
    }
    for field in ("title", "game_code", "revision"):
        if fields[field] != EXPECTED[field]:
            raise ValueError(
                f"unexpected header {field}: {fields[field]!r} "
                f"(expected {EXPECTED[field]!r})"
            )

    digests = digest_file(path)
    for name, expected in (
        ("md5", EXPECTED["md5"]),
        ("sha1", EXPECTED["sha1"]),
        ("sha256", EXPECTED["sha256"]),
    ):
        if digests[name] != expected:
            raise ValueError(
                f"unexpected {name}: {digests[name]} (expected {expected})"
            )
    return {**fields, **digests, "path": str(path)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="local user-owned AWRP00 .nds file")
    parser.add_argument("--json", action="store_true", help="emit one JSON result")
    args = parser.parse_args()

    try:
        result = validate(args.rom)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(result, sort_keys=True))
    else:
        print(f"validated {args.rom}")
        for field in ("size", "title", "game_code", "revision", "md5", "sha1", "sha256"):
            print(f"{field}: {result[field]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
