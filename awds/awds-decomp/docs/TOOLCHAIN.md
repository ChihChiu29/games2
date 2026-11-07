# Toolchain

## Pinned public tools

| Tool | Version | Linux asset/checksum |
| --- | --- | --- |
| `dsd` | `v0.12.0` | `dsd-linux-x86_64`, SHA-256 `03c129c2385b95a2c9545cafa975b41b5d49f62c7a5268f1452896d37883e217` |
| `objdiff-cli` | `v3.8.0` | `objdiff-cli-linux-x86_64`, SHA-256 `bc1e047126f9c6914bd1695798175234642ab9eaf45e886f841b59a4231e1a81` |
| `dsd-ghidra` | `v0.7.0` | optional Ghidra extension; verify its release asset before installation |
| `wibo` | `1.2.0` | optional Windows-program runner, only if the selected compiler package needs it |

`tools/download_tool.py` verifies the two executable assets whose checksums are pinned here. Ghidra and compiler payloads remain local and are not downloaded by public CI.

## Compiler decision

The Advance Wars compiler configuration was accepted on 2026-08-21 after byte-exact local matches for all three acceptance legs:

1. one known SDK/runtime signature: `Sqrt` at `0x02000214` (Thumb);
2. one ARM game-owned leaf function: `func_02004000` and `func_02004548` (two independent matches);
3. one Thumb game-owned leaf function: `func_0210cf0c` (assembled source, see below).

### Accepted configuration

| Field | Value |
| --- | --- |
| C compiler | `mwccarm.exe`, Metrowerks C/C++ for Embedded ARM 2.0 build 56 (`mwcc_20_56`) |
| C provenance | `decompme/compilers` release asset `mwcc_20_56.tar.gz`, kept outside tracked files; local SHA-256 `91e10b9826bb985641a6db075da444a51bf25dd53df3216cb8bc05548561ec45` |
| Assembler | `mwasmarm.exe` 1.0 build 20 (Freescale, Dec 2005) from the `mwccarm.zip` package, directory `mwccarm/2.0/base`, invoked with `-gnu` |
| Windows runner | `wibo` `1.2.0`, local SHA-256 `13f86a2d618f0dbe67179d349625345eabf9b46450295cb4c904e49f6aff85af` |
| C flags | `-O4 -proc arm946e -interworking -constpool -fp soft -opt speed -Cpp_exceptions off` |
| Thumb C override | add `-thumb` |
| Include paths | none required so far |
| Linker | not yet exercised; record `mwldarm` version and options at the first link (Phase 2) |
| Per-file overrides | `func_0210cf0c.s` is assembly because no recoverable MWCC build expresses its Thumb codegen from C (see below); assemble with `mwasmarm -gnu` |

`-Cpp_exceptions off` is evidence-based: a full-pattern scan of the ARM9 image found no MWCC exception-table records, and `dsd fix find-exceptix` confirms no exception table exists. With this flag built objects carry no `.exceptix` section, so whole-object comparisons are clean.

Local build loop: `python tools/configure.py build-units` delinks every unit declared in `config/AWRP00/arm9/delinks.txt`, infers each unit's instruction set from the delinked target's `$a`/`$t` mapping symbols, and compiles C with `mwcc_20_56` (adding `-thumb` when needed) and assembly with `mwasmarm -gnu` into `build/AWRP00/src/`. `python tools/configure.py report` then generates `build/AWRP00/report.json` from the regenerated `objdiff.json`.

Tested alternatives: all 24 current `nds_arm9` MWCC IDs were tested with the original flags, and all 26 binaries in the `mwccarm.zip` package (1.2 base/sp2–sp4, 2.0 base/sp1–sp2p4, dsi 1.1–1.6sp2) were tested locally against the refined flag set. The decomp.me `nds_arm9` IDs are aliases over the same `mwccarm.zip` directories (`mwcc_20_72`–`mwcc_20_87` → 1.2 line, `mwcc_30_114`–`mwcc_30_139` → 2.0 line, `mwcc_40_1018`–`mwcc_40_1051` → dsi line), so no additional compiler exists behind the platform.

Do not infer this configuration from another Intelligent Systems project; it was recovered from this target's own bytes.

### Thumb codegen findings (2026-08-21)

Rationale for the assembly override on `func_0210cf0c`, and constraints for future Thumb work:

- The target uses post-increment `stmia r0!` multi-stores, synthesizes `0x1000` as `mov r1,#1; lsl r1,#12`, and keeps a zero value in `r3`. Sibling functions `func_0210cf2c` and `func_0210cf48` share this profile.
- None of the 26 locally available MWCC builds (1.2 base/sp2–sp4, 2.0 base/sp1–sp2p4, dsi 1.1–1.6sp2) produces store merging into `stmia`, or shift synthesis instead of literal pooling, in Thumb mode under any tested combination of roughly 40 flag sets (`-O2/-O3/-O4`, `-opt speed|space`, bare levels, with/without `-constpool`, C99 compound literals, cursor/pair/temporary source shapes).
- The decomp.me `nds_arm9` compiler IDs are aliases over the same `mwccarm.zip` package (verified against each ID's Dockerfile in the `decompme/compilers` repository), so remote testing cannot expand the search space beyond the binaries tested locally.
- Several small "functions" reported by automatic analysis near the end of `.text` (for example `func_02129a12`, `func_02129100`, `func_0212cb86`) decode as veneer or data bytes, not Thumb code; their boundaries need re-analysis before they can be claimed.

Together these results indicate the target's Thumb family was written in assembly in the original build or compiled by a tool outside the recoverable MWCC set. Per [`CONTRIBUTING.md`](../CONTRIBUTING.md), assembly is permitted when the compiler cannot express the target; this section is that documentation for this function family.

## Local authority

`dsd` generates the linker configuration and `objdiff.json`; the checked-in `config/AWRP00/arm9/**` files remain the canonical analysis input. A unit is complete only when its locally built source object matches target bytes, relocations, and relevant data. decomp.me scores and Ghidra output are hypotheses, not acceptance evidence.

ARM7 uses a separate compiler fingerprint and build configuration. Reuse a package only after independent ARM7 byte evidence.

## Provisional local fingerprint

Superseded by the accepted configuration above; retained for the Thumb findings that motivated the assembly override.
