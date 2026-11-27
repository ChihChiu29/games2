# Repository Instructions (`games2`)

Universal conventions for all projects and game mods in this repository. Project-specific details (memory offsets, hooks, toolchains, anti-tamper workarounds, config keys, and changelogs) belong in each project's own `README.md` (or `<project>/AGENTS.md`).

## 1. Always Read Before Editing
- Before modifying or overwriting any file, **read the current file on disk** and check `git status` / `git log -n 5`.
- Never overwrite files from memory—changes may have been made and committed in another session.

## 2. Standard Project Structure
- **Game mods (`game_mods/<game>/`):** Place new binary/patch game mods under `game_mods/<game>/` by default.
- **Standalone / non-`game_mods` projects (`<short_game_name>/`):** Place standalone builds/ports/reimplementations in a short game-named directory at the repo root with this layout:
  - `VERSION` — plain-text current version number (e.g., `1.3`).
  - `build.sh` — builds the next version and packages reproducible outputs into `release/`.
  - `src/` — project source code.
  - `release/` — built artifacts (e.g., `desktop-<VERSION>.jar`, `*_mod<VERSION>.apk`, `dinput8.dll`, DOSBox-Pure `.zip` packs), standalone patchers, and user-facing `.ini` config files.
  - `README.md` — per-project usage, changelog, offsets/hooks, and build notes.
  - Additional project-specific directories as needed (e.g., `original_paks/`, `maps/`, `tools/`).
- Keep `game_mods/<game>/` projects self-contained with the same `VERSION`, `build.sh` (and/or patcher script), `release/`, and `README.md` conventions.

## 3. Versioning
- **Bump `VERSION` on every change** (code, patches, or config filename/schema changes).
- Wire `VERSION` into build output names (e.g., `desktop-<VERSION>.jar`, `*_mod<VERSION>.apk`) and in-mod version strings/logs where applicable.
- Add a short entry to the project's `README.md` changelog when bumping `VERSION`.

## 4. Verify Actual Outputs Before Reporting Done
Never report a build as "done" without verifying the actual generated artifact:
- **Java / `.jar` mods:** Confirm `META-INF/MANIFEST.MF`, target Java bytecode version (e.g., Java 8 / major 52 for StS), run `java -Xverify:all`, disassemble patched call sites with `javap` to verify exact method signatures, and execute helper methods in a JVM test.
- **Android `.apk` mods:** Run `zipalign -c 4`, run official `apksigner verify --verbose` (v1/v2/v3), read back the patched bytes/instructions from inside the final `.apk`, and check `AndroidManifest.xml`/DEX for anti-tamper wrappers (e.g., ApkVision `Dex2C`/`libAPKVISION.so`).
- **Windows `.dll` / Console `eboot.bin` mods:** Verify exported symbols / ELF headers / patched machine code in the final output.

## 5. Source Game Files Server
- User-provided source binaries, JARs, and APKs are hosted locally at `http://192.168.1.245:3000/`.
- Download needed files directly into their corresponding project subdirectory.

## 6. Git Hygiene
- **Never commit `.jar`, `.apk`, `.APK`, `.idsig`, `.exe`, `.EXE`, `.nds`, or `eboot*.bin` files.** Always keep large game binaries and built archives untracked via `.gitignore`.
- Clean up temporary test files/logs (e.g., `info.displayconfig`, `sts_cheat.log`) before committing.
