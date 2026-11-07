# Contributing

This is a matching-decompilation project. Contributions must improve a reproducible source build or the analysis needed to reach one.

## Target and legal rules

Use only the `AWRP00` target described in [`config/AWRP00/target.yaml`](config/AWRP00/target.yaml). Keep ROMs, archives, extracted files, target objects, filesystem dumps, copyrighted assets, ARM7 BIOS files, proprietary compilers, and decomp.me session credentials outside commits. The repository's ignore rules are a guard, not a substitute for checking staged files.

All source is written from scratch. Do not copy game code, generated decompilations, disassembly text, or asset data into source files. Analysis metadata may identify target addresses, symbols, ranges, and relocations.

## Claiming and matching a unit

1. Check the canonical `config/AWRP00/arm9` files and current `objdiff.json` before selecting a unit.
2. Claim one coherent unit or dependency chain in an issue or pull request. State whether it is ARM, Thumb, SDK/runtime, game-owned, or overlay code.
3. Recover or verify symbol, relocation, and data boundaries before changing source.
4. Write C/C++ from scratch. Use assembly only when the compiler cannot express the target and document why.
5. Build locally with the pinned compiler configuration and compare byte output, relocations, and relevant data using `objdiff`.
6. Include the exact compiler ID, flags, tool versions, analysis-file changes, and a local report excerpt in the pull request.

A decomp.me score, a Ghidra decompile, a target-only object, copied ARM7 bytes, or a generated extraction does not count as source coverage.

## Pull requests

Keep a pull request to one coherent unit or one tightly coupled dependency chain. Do not mix boundary recovery with unrelated formatting. Reviewers must be able to reproduce the result from a clean checkout plus their own local ROM and licensed toolchain.

Before submitting:

```sh
python -m compileall -q tools
python tools/validate_rom.py extract/baserom_awds_AWRP00.nds
```

If the ROM or proprietary compiler is unavailable, run the no-ROM checks and explain which local checks could not run. Never upload the missing inputs to CI, an issue, a pull request, or decomp.me.

## decomp.me

Use only sanitized single-function ARM9 experiments. Treat all submitted assembly, context, scratch links, and compiler metadata as public. Never upload a ROM, full overlay, filesystem/assets, secrets, or repository session files. The local link and `objdiff` result are authoritative.

## Conduct

Be precise, patient, and evidence-driven. Disagree with code or analysis, not people. Harassment, discrimination, doxxing, credential sharing, and redistribution of copyrighted target material are not acceptable.
