#!/usr/bin/env python3
"""Download and verify public tools pinned by this project."""

from __future__ import annotations

import argparse
import hashlib
import os
import sys
import tempfile
from pathlib import Path
from urllib.request import Request, urlopen

TOOLS = {
    "dsd": {
        "version": "0.12.0",
        "asset": "dsd-linux-x86_64",
        "url": "https://github.com/AetiasHax/ds-decomp/releases/download/v0.12.0/dsd-linux-x86_64",
        "sha256": "03c129c2385b95a2c9545cafa975b41b5d49f62c7a5268f1452896d37883e217",
    },
    "objdiff-cli": {
        "version": "3.8.0",
        "asset": "objdiff-cli-linux-x86_64",
        "url": "https://github.com/encounter/objdiff/releases/download/v3.8.0/objdiff-cli-linux-x86_64",
        "sha256": "bc1e047126f9c6914bd1695798175234642ab9eaf45e886f841b59a4231e1a81",
    },
    "dsd-ghidra": {
        "version": "0.7.0",
        "asset": None,
        "url": "https://github.com/AetiasHax/dsd-ghidra/releases/tag/v0.7.0",
        "sha256": None,
    },
    "wibo": {
        "version": "1.2.0",
        "asset": None,
        "url": None,
        "sha256": None,
    },
}


def print_pins() -> None:
    for name, pin in TOOLS.items():
        checksum = pin["sha256"] or "manual release verification required"
        print(f"{name}: v{pin['version']} {checksum}")


def download(name: str, output: Path, force: bool) -> None:
    pin = TOOLS[name]
    url = pin["url"]
    expected = pin["sha256"]
    if not url or not expected or not pin["asset"]:
        raise ValueError(
            f"{name} has no pinned executable asset; install it manually from {url or 'its official release'}"
        )
    if output.exists() and not force:
        actual = hashlib.sha256(output.read_bytes()).hexdigest()
        if actual == expected:
            print(f"already verified: {output}")
            return
        raise ValueError(f"existing file has unexpected SHA-256: {output}")

    output.parent.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha256()
    with tempfile.NamedTemporaryFile(
        mode="wb", prefix=f".{output.name}.", dir=output.parent, delete=False
    ) as temporary:
        temporary_path = Path(temporary.name)
        try:
            request = Request(url, headers={"User-Agent": "awds-decomp-tool-downloader"})
            with urlopen(request, timeout=60) as response:
                while chunk := response.read(1024 * 1024):
                    digest.update(chunk)
                    temporary.write(chunk)
            actual = digest.hexdigest()
            if actual != expected:
                raise ValueError(f"downloaded SHA-256 {actual}, expected {expected}")
            temporary.flush()
            os.chmod(temporary_path, 0o755)
            temporary_path.replace(output)
        except Exception:
            temporary_path.unlink(missing_ok=True)
            raise
    print(f"installed {name} v{pin['version']} at {output}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tool", nargs="?", choices=sorted(TOOLS))
    parser.add_argument("--output", type=Path, help="destination executable")
    parser.add_argument("--force", action="store_true", help="replace a mismatched existing file")
    parser.add_argument("--list", action="store_true", help="print all pinned tool metadata")
    args = parser.parse_args()

    if args.list:
        print_pins()
        return 0
    if not args.tool:
        parser.error("choose a tool or pass --list")
    if not args.output:
        parser.error("--output is required when downloading")
    try:
        download(args.tool, args.output, args.force)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
