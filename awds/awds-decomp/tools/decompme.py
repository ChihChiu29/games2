#!/usr/bin/env python3
"""Create a deliberately small, reviewable ARM9 decomp.me handoff package."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

MAX_ASSEMBLY_BYTES = 64 * 1024
MAX_CONTEXT_BYTES = 32 * 1024
FORBIDDEN_MARKERS = (
    "baserom",
    ".nds",
    "extract/",
    "build/",
    "filesystem",
    "romfs",
    "arm7_bios",
    "license.dat",
    ".git/",
)
LABEL_RE = re.compile(r"^([A-Za-z_.$][A-Za-z0-9_.$]*)\s*:\s*$")
LOCAL_LABEL_RE = re.compile(r"^\.L[A-Za-z0-9_.$]*\s*:\s*$")
DIRECTIVE_PREFIXES = (".syntax", ".fpu")
MODE_PREFIXES = (".arm", ".thumb", ".code")
BRANCH_OPS = {
    "b", "beq", "bne", "bcs", "bhs", "bcc", "blo", "bmi", "bpl",
    "bvs", "bvc", "bhi", "bls", "bge", "blt", "bgt", "ble", "bal",
    "bnv", "bl", "blx",
}


def reject_forbidden(text: str, source: str) -> None:
    lowered = text.lower()
    found = [marker for marker in FORBIDDEN_MARKERS if marker in lowered]
    if found:
        joined = ", ".join(found)
        raise ValueError(f"{source} contains forbidden target/material marker(s): {joined}")


def extract_function(text: str, function: str) -> str:
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        match = LABEL_RE.match(line.strip())
        if match and match.group(1) == function:
            start = index
            break
    if start is None:
        raise ValueError(f"function label not found: {function}:")

    end = len(lines)
    for index in range(start + 1, len(lines)):
        stripped = lines[index].strip()
        match = LABEL_RE.match(stripped)
        if match and not LOCAL_LABEL_RE.match(stripped):
            end = index
            break
    body = "\n".join(lines[start:end]).strip() + "\n"
    if not body.strip():
        raise ValueError(f"function body is empty: {function}")
    return body


def extract_directives(text: str, function: str) -> list[str]:
    lines = text.splitlines()
    start = None
    for index, line in enumerate(lines):
        match = LABEL_RE.match(line.strip())
        if match and match.group(1) == function:
            start = index
            break
    if start is None:
        raise ValueError(f"function label not found: {function}:")

    directives: list[str] = []
    for line in lines[: start + 1]:
        stripped = line.strip()
        if stripped.startswith(DIRECTIVE_PREFIXES):
            if stripped not in directives:
                directives.append(stripped)
        elif stripped.startswith(MODE_PREFIXES):
            directives = [item for item in directives if not item.startswith(MODE_PREFIXES)]
            directives.append(stripped)
    return directives


def collect_relocation_labels(assembly: str) -> set[str]:
    labels: set[str] = set()
    label_pattern = r"[A-Za-z_.$][A-Za-z0-9_.$]*"
    for raw_line in assembly.splitlines():
        line = raw_line.split(";", 1)[0].strip()
        if not line or line.startswith("."):
            continue
        parts = line.split(None, 1)
        if len(parts) != 2:
            continue
        opcode, operands = parts[0].lower(), parts[1].strip()
        if opcode in BRANCH_OPS:
            operand = operands.split(",", 1)[0].strip().lstrip("#")
            if re.fullmatch(label_pattern, operand):
                labels.add(operand)
        elif opcode == "adr" or opcode.startswith("ldr"):
            if "," not in operands:
                continue
            operand = operands.split(",", 1)[1].strip().lstrip("=")
            if re.fullmatch(label_pattern, operand):
                labels.add(operand)
    return labels


def prepare(args: argparse.Namespace) -> int:
    assembly_path = args.assembly.resolve()
    if not assembly_path.is_file():
        raise ValueError(f"assembly file is missing: {assembly_path}")
    if assembly_path.stat().st_size > MAX_ASSEMBLY_BYTES:
        raise ValueError(f"assembly file exceeds {MAX_ASSEMBLY_BYTES} bytes")
    assembly = assembly_path.read_text(encoding="utf-8")
    reject_forbidden(assembly, "assembly")
    function_assembly = extract_function(assembly, args.function)
    reject_forbidden(function_assembly, "selected function")

    context = ""
    if args.context:
        context_path = args.context.resolve()
        if not context_path.is_file():
            raise ValueError(f"context file is missing: {context_path}")
        if context_path.stat().st_size > MAX_CONTEXT_BYTES:
            raise ValueError(f"context file exceeds {MAX_CONTEXT_BYTES} bytes")
        context = context_path.read_text(encoding="utf-8")
        reject_forbidden(context, "context")

    directives = extract_directives(assembly, args.function)
    relocation_labels = sorted(collect_relocation_labels(function_assembly))
    literal_pool = [
        line.strip()
        for line in function_assembly.splitlines()
        if line.strip().startswith((".word", ".4byte", ".long", ".short"))
    ]
    package = {
        "schema": "awds-decompme-v1",
        "function": args.function,
        "compiler": args.compiler,
        "flags": args.flags,
        "diff_label": args.diff_label,
        "directives": directives,
        "relocation_labels": relocation_labels,
        "literal_pool": literal_pool,
        "assembly": function_assembly,
        "context": context,
        "public_payload_review_required": True,
    }
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(package, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote sanitized handoff: {output}")
    print("review the JSON for target-derived material before any public upload")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    prepare_parser = subparsers.add_parser("prepare")
    prepare_parser.add_argument("--assembly", type=Path, required=True)
    prepare_parser.add_argument("--function", required=True)
    prepare_parser.add_argument("--compiler", required=True)
    prepare_parser.add_argument("--flags", required=True)
    prepare_parser.add_argument("--context", type=Path)
    prepare_parser.add_argument("--diff-label", default="awds")
    prepare_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        return prepare(args)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
