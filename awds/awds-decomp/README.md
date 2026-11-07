# Advance Wars: Dual Strike — matching decompilation

`awds-decomp` is a from-scratch matching-decompilation project for the European revision 0 Nintendo DS release of *Advance Wars: Dual Strike*.

## Target

| Field | Value |
| --- | --- |
| Version | `AWRP00` |
| Game code | `AWRP` |
| Header title | `ADVANCEWARSP` |
| Revision | `0` |
| Size | `33,554,432` bytes |
| SHA-1 | `03b63875930bd8fdea50902d7d2e0a4614ac36eb` |
| SHA-256 | `a16d29f4a41748999a573dc24dba1119512ae6fa558645147b42dab5f51ad50b` |

The complete target manifest is [`config/AWRP00/target.yaml`](config/AWRP00/target.yaml). The ROM and every extracted or target-derived file are local inputs and are never committed.

## Legal boundary

This repository contains contributor-authored source, analysis metadata, and tooling only. Nintendo's game code, graphics, audio, text, ROM filesystem, extracted binaries, and other original assets are not distributed or relicensed here. See [`LICENSE`](LICENSE) and [`CONTRIBUTING.md`](CONTRIBUTING.md).

## Local bootstrap

Install Python 3.10 or newer, then place the user-owned ROM at `extract/baserom_awds_AWRP00.nds`. The filename is only a convention; the validator checks the contents, not the name.

```sh
python tools/validate_rom.py extract/baserom_awds_AWRP00.nds
python tools/download_tool.py dsd --output tools/dsd
python tools/download_tool.py objdiff-cli --output tools/objdiff-cli
./tools/dsd rom extract \
  --rom extract/baserom_awds_AWRP00.nds \
  --output-path extract/AWRP00
./tools/dsd init \
  --rom-config extract/AWRP00/config.yaml \
  --output-path config/AWRP00 \
  --build-path build/AWRP00 \
  --allow-unknown-function-calls
```

The hidden `--allow-unknown-function-calls` option is intentional for this initial analysis: automatic discovery misses call destinations in this target. It must not be replaced by deleting unresolved symbols.

Apply the canonical analysis passes:

```sh
./tools/dsd sig apply --config-path config/AWRP00/arm9/config.yaml --all
./tools/dsd fix dsprot --config-path config/AWRP00/arm9/config.yaml --dry
./tools/dsd fix ov-sigs --config-path config/AWRP00/arm9/config.yaml --dry
./tools/dsd fix find-exceptix --config-path config/AWRP00/arm9/config.yaml --dry
./tools/dsd delink --config-path config/AWRP00/arm9/config.yaml
./tools/dsd lcf --config-path config/AWRP00/arm9/config.yaml
```

`config/AWRP00/arm9/**` is the canonical analysis source. Ghidra is an inspection client; synchronize it with `dsd-ghidra` and do not treat a Ghidra database as source control.

## Project status

The first analysis inventory covers ARM9 main, ITCM, DTCM, and 12 ARM9 overlays. ARM9 overlays `ov000`–`ov004` need manual function-boundary recovery. ARM7 is a separate later source target because the available `dsd` and decomp.me workflows are ARM9-first. Target-only inventory is not counted as decompiled progress. See [`docs/PROGRESS.md`](docs/PROGRESS.md).

The compiler configuration was recovered from the target's own bytes and accepted on 2026-08-21: `mwcc_20_56` for C (`-O4 -proc arm946e -interworking -constpool -fp soft -opt speed -Cpp_exceptions off`, plus `-thumb` where needed) and `mwasmarm 2.0/base -gnu` for the assembly units whose Thumb codegen no MWCC build expresses from C. Four matched units (SDK/runtime, two ARM, one Thumb) are wired into the build loop via `python tools/configure.py build-units`; the first local progress report records 4 matched functions and 98 matched ARM9 code bytes. See [`docs/TOOLCHAIN.md`](docs/TOOLCHAIN.md) and [`docs/PROGRESS.md`](docs/PROGRESS.md).

## Contributor workflow

Read [`CONTRIBUTING.md`](CONTRIBUTING.md) before claiming a unit. Every accepted unit must match locally at byte, relocation, and relevant-data level. decomp.me is an optional ARM9 scratchpad only; its score never replaces local `objdiff` evidence. See [`docs/DECOMP_ME.md`](docs/DECOMP_ME.md).

## Public automation

- `checks.yml` validates repository policy without a ROM or proprietary compiler.
- `report.yml` is for a protected trusted `main` runner with local target/compiler inputs. It uploads only `AWRP00_report` containing `report.json` for decomp.dev.

The final project gate is a deterministic rebuilt ROM whose SHA-1 equals the target manifest, with ARM9, all overlays, ARM7, symbols, modules, relocations, and filesystem layout accounted for.
