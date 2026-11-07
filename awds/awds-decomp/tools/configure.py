#!/usr/bin/env python3
"""Run reproducible local dsd/objdiff actions without embedding a compiler guess."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DSD = ROOT / "tools" / "dsd"
OBJDIFF = ROOT / "tools" / "objdiff-cli"
CONFIG = ROOT / "config" / "AWRP00" / "arm9" / "config.yaml"
BUILD = ROOT / "build" / "AWRP00"
COMPILER_DIR = ROOT / "scratch" / "AWRP00" / "compiler"
WIBO = COMPILER_DIR / "wibo"
MWCC = COMPILER_DIR / "mwcc_20_56" / "mwccarm.exe"
MWASM = COMPILER_DIR / "catalog" / "mwccarm" / "2.0" / "base" / "mwasmarm.exe"
C_FLAGS = [
    "-O4",
    "-proc",
    "arm946e",
    "-interworking",
    "-constpool",
    "-fp",
    "soft",
    "-opt",
    "speed",
    "-Cpp_exceptions",
    "off",
]


def require_file(path: Path, description: str) -> None:
    if not path.is_file():
        raise SystemExit(f"{description} is missing: {path}")


def run(command: list[str]) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=ROOT, check=True)


def show_status() -> int:
    rom = ROOT / "extract" / "baserom_awds_AWRP00.nds"
    print(f"root: {ROOT}")
    print(f"ROM present: {rom.is_file()} ({rom})")
    print(f"dsd present: {DSD.is_file()} ({DSD})")
    print(f"objdiff-cli present: {OBJDIFF.is_file()} ({OBJDIFF})")
    print(f"ARM9 config present: {CONFIG.is_file()} ({CONFIG})")
    print("compiler: accepted (mwcc_20_56); see docs/TOOLCHAIN.md")
    return 0


def generate_objdiff(args: argparse.Namespace) -> int:
    require_file(DSD, "dsd")
    require_file(CONFIG, "ARM9 dsd configuration")
    if not args.compiler:
        raise SystemExit("--compiler is required; do not publish a guessed compiler")
    command = [
        str(DSD),
        "objdiff",
        "--config-path",
        str(CONFIG.relative_to(ROOT)),
        "--output-path",
        ".",
        "--skip-absent-objects",
        "--scratch",
        "--compiler",
        args.compiler,
    ]
    if args.c_flags:
        command.extend(["--c-flags", args.c_flags])
    run(command)
    return 0


def generate_report(_: argparse.Namespace) -> int:
    require_file(OBJDIFF, "objdiff-cli")
    output = ROOT / "build" / "AWRP00" / "report.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    run([
        str(OBJDIFF),
        "report",
        "generate",
        "--project",
        ".",
        "--output",
        str(output.relative_to(ROOT)),
        "--format",
        "json",
    ])
    return 0


def delink_units() -> list[str]:
    """Return unit source paths (relative to root) declared in the delinks manifest."""
    delinks = CONFIG.parent / "delinks.txt"
    require_file(delinks, "delinked unit manifest")
    sources = []
    for line in delinks.read_text().splitlines():
        entry = line.strip()
        if not entry.endswith(":") or line.startswith((" ", "\t")):
            continue
        source = entry[:-1]
        if source.endswith((".c", ".s")) and (ROOT / source).is_file():
            sources.append(source)
    return sources


def target_is_thumb(target_object: Path) -> bool:
    """Read mapping symbols ($a/$t) from a delinked ELF object."""
    import struct

    data = target_object.read_bytes()
    if data[:4] != b"\x7fELF":
        raise SystemExit(f"{target_object} is not an ELF object")
    fields = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    shoff = fields[6]
    shentsize = fields[11]
    shnum = fields[12]
    symtab = None
    strtab_size = None
    strtab_off = None
    for index in range(shnum):
        _, shtype, _, _, offset, size, link, _, _, sh_entsize = struct.unpack_from(
            "<10I", data, shoff + index * shentsize
        )
        if shtype == 2:  # SHT_SYMTAB
            symtab = (offset, size, sh_entsize)
            strtab = struct.unpack_from("<10I", data, shoff + link * shentsize)
            strtab_off, strtab_size = strtab[4], strtab[5]
            break
    if symtab is None:
        raise SystemExit(f"{target_object} has no symbol table")
    offset, size, entsize = symtab

    def symbol_name(st_name: int) -> str:
        start = strtab_off + st_name
        return data[start : data.index(b"\0", start)].decode()

    has_thumb = False
    has_arm = False
    for pos in range(offset, offset + size, entsize):
        st_name, st_value, _size, st_info, _other, _shndx = struct.unpack_from("<IIIBBH", data, pos)
        name = symbol_name(st_name)
        if name == "$t":
            has_thumb = True
        elif name == "$a":
            has_arm = True
        elif st_info >> 4 == 1 and st_info & 0xF == 2 and st_value & 1:
            has_thumb = True
    if not has_thumb and not has_arm:
        raise SystemExit(f"{target_object} exposes no ISA markers")
    return has_thumb and not has_arm


def build_units(_: argparse.Namespace) -> int:
    for path in (WIBO, MWCC, MWASM):
        require_file(path, f"pinned compiler component {path.name}")
    run([str(DSD), "delink", "--config-path", str(CONFIG.relative_to(ROOT))])
    sources = delink_units()
    if not sources:
        raise SystemExit("no compilable units declared in the delinks manifest")
    env = dict(os.environ, MWCINCLUDES=".")
    built = 0
    for source in sources:
        rel_parts = Path(source).parts[1:]  # strip the leading src/
        stem = Path(*rel_parts).with_suffix(".o").as_posix()
        target_object = BUILD / "delinks" / "src" / stem
        require_file(target_object, f"delinked target object for {source}")
        thumb = target_is_thumb(target_object)
        output = BUILD / "src" / stem
        output.parent.mkdir(parents=True, exist_ok=True)
        relative_source = Path("..", "..", "..") / source
        relative_output = Path("..", "..", "..") / output.relative_to(ROOT).as_posix()
        if source.endswith(".c"):
            command = [str(WIBO), str(MWCC), "-c", str(relative_source), "-o", str(relative_output)]
            command.extend(C_FLAGS)
            if thumb:
                command.append("-thumb")
        else:
            command = [
                str(WIBO),
                str(MWASM.relative_to(COMPILER_DIR)),
                "-gnu",
                str(relative_source),
                "-o",
                str(relative_output),
            ]
        print(f"+ [{'thumb' if thumb else 'arm '}] {' '.join(command)}")
        subprocess.run(command, cwd=COMPILER_DIR, env=env, check=True)
        built += 1
    print(f"built {built} unit object(s) under {BUILD / 'src'}")
    return 0


def build_rom(args: argparse.Namespace) -> int:
    require_file(DSD, "dsd")
    require_file(args.elf, "ARM9 ELF")
    require_file(CONFIG, "ARM9 dsd configuration")
    run([
        str(DSD),
        "rom",
        "config",
        "--elf",
        str(args.elf),
        "--config",
        str(CONFIG.relative_to(ROOT)),
    ])
    rom_config = BUILD / "build" / "rom_config.yaml"
    require_file(rom_config, "generated ROM configuration")
    output = args.output or BUILD / "AWRP00.nds"
    run([
        str(DSD),
        "rom",
        "build",
        "--config",
        str(rom_config.relative_to(ROOT)),
        "--rom",
        str(output.relative_to(ROOT)),
    ])
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("status", help="show local inputs and toolchain state")
    subparsers.add_parser("build-units", help="compile delinked unit sources with the accepted configuration")
    objdiff = subparsers.add_parser("objdiff", help="generate dsd's objdiff project")
    objdiff.add_argument("--compiler")
    objdiff.add_argument("--c-flags")
    subparsers.add_parser("report", help="generate the local JSON progress report")
    rom = subparsers.add_parser("rom", help="link a local ARM9 ELF and build a ROM")
    rom.add_argument("--elf", type=Path, required=True)
    rom.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "status":
            return show_status()
        if args.command == "build-units":
            return build_units(args)
        if args.command == "objdiff":
            return generate_objdiff(args)
        if args.command == "report":
            return generate_report(args)
        if args.command == "rom":
            return build_rom(args)
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
