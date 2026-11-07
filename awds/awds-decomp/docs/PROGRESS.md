# Progress

## Target

This project targets `AWRP00`, the European revision 0 image described in [`config/AWRP00/target.yaml`](../config/AWRP00/target.yaml). The target ROM is local-only.

## Analysis baseline

The initial `dsd v0.12.0` analysis produced:

- ARM9 main load/entry: `0x02000000` / `0x02000800`;
- ARM7 load/entry: `0x02380000`;
- ARM9 main, ITCM, DTCM, and 12 ARM9 overlay units;
- 7,498 discovered ARM9 functions;
- 1,698,382 target ARM9 code bytes;
- 4,738,200 target ARM9 data bytes.

Automatic analysis found no functions in `ov000`–`ov004`. These are unresolved boundary-analysis gaps, not empty overlays. Their constructor ranges, relocation tables, call targets, and ARM/Thumb entry points must be recovered manually.

The generated inventory is target-only until source-built objects are wired into `objdiff.json` and the DSD link loop. The compiler configuration (`mwcc_20_56`) was accepted on 2026-08-21; see [`docs/TOOLCHAIN.md`](TOOLCHAIN.md).

## Current local matching work

The local source hypotheses are, all verified against their delinked target units with `objdiff-cli diff` at 100%, including whole-object `.text` equality and relocations where present:

- `Sqrt` at `0x02000214`: Thumb SDK/runtime stub (`swi #0xd; bx lr`), C source, `mwcc_20_56` with `-thumb`.
- `func_02004000` at `0x02004000`: ARM game-owned function, C source, `mwcc_20_56`.
- `func_02004548` at `0x02004548`: ARM game-owned function, C source, `mwcc_20_56`.
- `func_0210cf0c` at `0x0210cf0c`: Thumb game-owned function, from-scratch assembly source (`mwasmarm -gnu`); assembly is documented in [`docs/TOOLCHAIN.md`](TOOLCHAIN.md) because no recoverable MWCC build expresses this codegen from C.

These units are wired into the canonical build loop: `python tools/configure.py build-units` delinks and compiles every declared unit source with the accepted configuration (C via `mwcc_20_56`, assembly via `mwasmarm -gnu`, with `-thumb` inferred from the target's mapping symbols), and `python tools/configure.py report` produces `build/AWRP00/report.json`. The first project-level report records 4 of 7,498 functions and 98 of 1,698,382 ARM9 code bytes matched (0.0058%).

The ARM9 ELF link loop is not exercised yet, so `dsd check modules --fail` still reports checksum failures for all target modules; this is expected until the linker configuration is recovered (Phase 3).

## Coverage policy

A unit counts only when source written from scratch builds to matching target bytes, relocations, and relevant data. Target-only objects, gap objects, generated extraction data, copied original ARM7 bytes, and unreviewed assembly do not count.

## Acceptance gates

1. ROM validation and deterministic `dsd` extraction/init pass without tracking ROM bytes.
2. Canonical symbols, relocations, and delinks cover every known ARM9/ITCM/DTCM/overlay range; the five automatic-analysis gaps remain explicitly tracked.
3. One SDK/runtime, one ARM game, and one Thumb game function match using one pinned local compiler configuration.
4. The first source unit reaches 100% local `objdiff` match, including relocations/data.
5. A trusted main-branch workflow uploads `AWRP00_report`; decomp.dev accepts it after the 0.5% visibility threshold (currently approximately 8,492 code bytes).
6. ARM9 main, ITCM/DTCM, all 12 overlays, and ARM7 have explicit source/target status.
7. `dsd check modules --fail`, `dsd check symbols --fail`, local `objdiff`, and rebuilt-ROM SHA-1 all pass.
8. Decompedia's category/project pages render and link back to the public repository after maintainer merge.

Current blockers are recorded in the implementation status, not represented as completed matches.

## Implementation status

Completed locally and published:

- Public repository: [ozyz/awds-decomp](https://github.com/ozyz/awds-decomp), public with issues enabled, `main` as the default branch, and the planned topics.
- Public `checks` workflow run `32404438436` completed successfully.
- Decompedia integration [PR #5](https://github.com/decompals/decompedia/pull/5) was prepared and then closed pending owner review; its fork branch remains available for a later submission.
- Local compiler testing covered all 24 current `nds_arm9` MWCC IDs with the original flag set, and all 26 binaries in the `mwccarm.zip` package locally against the refined flags; the decomp.me `nds_arm9` IDs were shown to alias those same binaries.
- The compiler configuration is accepted as of 2026-08-21: all three acceptance legs match locally under one pinned configuration (`mwcc_20_56` C, `mwasmarm 2.0/base -gnu` assembly). See [`docs/TOOLCHAIN.md`](TOOLCHAIN.md).

Remaining external and technical blockers:

- The `decomp.dev report` run `32404438588` is queued because no `awds-rom` self-hosted runner is configured. No report artifact has been uploaded and the project has not been registered on decomp.dev.
- The local report (`build/AWRP00/report.json`, not uploaded) is non-zero: 4 matched functions, 98 matched code bytes.
- The ARM9 ELF link loop is not recovered yet. `dsd check modules --fail` reports checksum failures for all target modules, and `dsd check symbols` needs a linked `arm9.elf`; both are expected to pass only after the linker configuration is recovered.
- ARM7 has no source build yet, and `ov000`–`ov004` still need manual function-boundary recovery.
- Several small "functions" near the end of `.text` (`func_02129a12`, `func_02129100`, `func_0212cb86`, and siblings) decode as veneer or data bytes and need boundary re-analysis.
- Branch protection and decomp.dev GitHub App installation remain intentionally deferred until a trusted report workflow can run successfully.
