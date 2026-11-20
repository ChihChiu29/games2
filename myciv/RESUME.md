# myciv — Project Resume Context

**Current Release:** `v480.07` → `myciv/release/civ_480.07_dospure.zip`  
**Base Game:** Sid Meier's Civilization (1991, DOS v475.01, `myciv/original/civ1_dos.zip`)  
**Engine:** `myciv/civ1_recom/` (static x86-to-C recompilation, cross-compiled to DOS DPMI via DJGPP + embedded `CWSDSTUB.EXE` for **DOSBox Pure**)  
*(Note: `myciv/civr/` is an unused repo; all active work is in `myciv/civ1_recom/`.)*

---

## Build & Versioning

- **Build command:**
  ```bash
  bash /home/chih/Workspace/git/games2/myciv/build.sh
  ```
- **Toolchain:** DJGPP at `/home/chih/Workspace/git/games2/NH5/NetHack/lib/djgpp` (with `CWSDSTUB.EXE` / `CWSDPMI.EXE`).
- **Output:** `myciv/release/civ_<version>_dospure.zip`
  - Contains `CIVR.EXE` (recompiled binary), original `CIV.EXE` (required alongside it), game assets from `original/civ1_dos.zip`, `DOSBOX.BAT` (auto-launches `CIVR.EXE`), and DOS-tuned `CIV.CFG`:
    - `gamedir = .`, `widescreen = 0`, `mapwidth = 160` (max 2× wider 160×50 world), `realtime = 0` (starts Turn-Based), `turnsecs = 3`, `barbarians = 0`, `rankings = 0`, `sound = speaker`, `mouse = 1`.
- **Versioning rule:**
  - **New feature:** increment integer (`476` → `477` → `478` → `479` → `480`).
  - **Bug fix:** increment `.01` suffix (`480.01` → `480.07`).
- **Important:** Do **not** run `make lift` (Python `capstone`/`unicorn` not installed; `src/gen/` is pre-generated). Incremental `make dos` via `build.sh` takes ~2–5 seconds (or ~20s if `root_0402.c` is touched).

---

## Features & Fixes Implemented (`476` → `480.07`)

1. **DOS Cross-Compile & Packaging (`v476`)**
   - Parameterized `CWSDSTUB` in `civ1_recom/Makefile`.
   - Guarded `<zlib.h>` with `#ifndef NO_PNG` in `src/native/mapsize.c` (uncompressed `"CIVW0"` in `CIVILn.WMP`) and `src/native/mp.c`.
   - Fixed `e10_dissolve()` stride bug in `runtime/gfx.c` (page 1/2 are 640px wide when `mapwidth = 160` while screen page 0 is 320px wide; previously sheared Palace / Finish Building / Wonder dissolves).

2. **Mouse Controls (`v477`, `v478.01`, `v479.01`, `v479.02`, `v480.06`)**
   - **Left-click on big map (`root_0402.c` `L_0caf`):** When a unit is active (and not in View mode), left-clicking an empty/terrain/enemy/unexplored tile issues **Goto** instead of centering the map (respects land/sea domain on explored tiles; allows exploring fog of war). Clicking own city still opens city screen; at End of Turn or in View mode, left-click still centers map.
   - **Left-click on friendly unit (`root_0402.c` `L_0c23`, `realtime.c` `map_click_select_unit`):** Directly selects & wakes friendly units (works in Turn-Based & Real-Time, even at End of Turn), cycles through stacks, cancels `autowork` on automated Settlers (`autowork_cancel()`), and shows a toast if 0 moves left.
   - **Right-click on big map (`root_0402.c` `L_0842`, `mapnav.c`):** Right-click on an adjacent tile moves 1 step; right-click on a non-adjacent tile inspects terrain (never Goto); right-click drag (>4px) scrolls the map smoothly (left-click drag also scrolls).

3. **On-Screen Real-Time Mode Toggle & End-of-Turn (`v478`, `v478.01`)**
   - Always-visible clickable Civ1-styled beveled button at the bottom of the left status panel (`RT_BTN_X=2, W=76, H=10, Y=H-11` in `overlay.c` / `realtime.c`).
   - Cycles **Turn-Based → 1X (3.0s/turn) → 2X (1.5s/turn) → Turn-Based**.
   - Game starts in Turn-Based (`realtime = 0`), and `hook_realtime_end_of_turn()` always waits at "End of Turn" (prevents Civ1's default auto-end-turn).

4. **City Screen UI & Development Governor (`v478.01`, `v479`, `v479.01`, `v480.02`, `v480.04`, `v480.07`)**
   - Classic Civ1 3D beveled button style (`civ_button()` matching `f_0d07_703b`) for `GOV`, `GOLD`, `TURN-BASED`/`1X`/`2X`, and toast banners in `src/native/overlay.c`.
   - When producing **Gold**, city screen covers old build icon/name with **`GOLD`** and displays a **`PRODUCING GOLD`** plaque in the shields box (hidden while *"What shall we build?"* is open).
   - **City Governor (`GOV` button at `x=192, y=189` in `capital.c`, `root_0ad5.c`):**
     - Skips *"What shall we build?"* popup on build completion (manual **Change** in city screen still works).
     - Priority: 1 garrison defender if 0 present → Aqueduct (if size ≥ 7) → Granary, Temple, Marketplace, Library, Aqueduct (size ≥ 5), Courthouse (non-capital), Cathedral, Bank, University, Colosseum, Factory, Hydro/Power Plant, Mfg. Plant, Mass Transit, Recycling Center, City Walls → **Produce Gold** when all development buildings are done (handles Page 2 `0x63` `"More..."` in late game!).
     - *(Known optional enhancement: Governor cities currently stay on Gold until manually toggled if a new tech unlocks a new building later.)*

5. **Unit XP, Level-Up & Hero Ascension (`v480` → `v480.07`)**
   - Implemented in `src/native/unitxp.c`, hooked in `root_1a19.c` (combat), `root_0402.c` (turn-start move refresh), `root_0860.c` (`f_0860_0ee0` universal unit destroy), `overlay.c` (status panel badge right above mode toggle).
   - Applies to **both human and AI** units; persisted in `CIVILn.NAT` (`"CIVN3"`, backward-compatible with `"CIVN1"`/`"CIVN2"`).
   - **Every 3 wins:** `+1 Level` and `+1` random stat (`Atk`, `Def`, or `Mov`; `Mov` capped at `+3`; non-combat `base_atk == 0` units only roll `Def`/`Mov`).
   - **Every 3 Levels (Lv3, Lv6, Lv9... — Hero Ascension):** Auto-Veteran + 1 random Hero perk:
     - **a.** Remove support need (`UNIT_HOME = 0xFF`, skipped if already support-free)
     - **b.** `+1` random stat to self + all friendly units within 1 tile (3×3, east/west wrapped)
     - **c.** `+1` random stat on self ×3
     - **d.** `+1` Extra Life (survives lethal combat blow; returns `AX = 0xFFFF` from `f_1a19_000e` to cleanly abort advance/capture and deducts attacker moves / zeroes Bomber moves).

---

## Key Modified Files (`myciv/civ1_recom/`)

- `Makefile` — parameterized `CWSDSTUB`
- `runtime/gfx.c` — `e10_dissolve()` wide-page stride fix
- `src/native/addons.h` — UI button coordinates & native declarations
- `src/native/autowork.c` — `autowork_cancel()`
- `src/native/capital.c` — Gold UI state, City Governor, `CIVN3` save/load, new-game reset
- `src/native/mapnav.c` — left & right drag hold-back
- `src/native/mapsize.c` & `src/native/mp.c` — `#ifndef NO_PNG` guards
- `src/native/overlay.c` — Civ1 3D beveled `GOV`, `GOLD`, `PRODUCING GOLD`, `TURN-BASED`/`1X`/`2X`, Unit XP/Hero badge, toast banners, `DS = DGROUP` guard
- `src/native/realtime.c` — `realtime_cycle()`, `map_click_select_unit()`, always-wait End of Turn
- `src/native/unitxp.c` — Unit XP, Level-Up, Hero perks, Extra Life, badge formatting, save/load
- `src/gen/funcs.h` — native helper declarations
- `src/gen/root_0402.c` — turn-start `+Mov` (`00c4`), left-click unit select (`L_0c23`, 20 copies), left-click Goto (`L_0caf`, 20 copies)
- `src/gen/root_0860.c` — `unit_xp_clear()` in `f_0860_0ee0`
- `src/gen/root_0ad5.c` — `governor_pick_build()` hook (with Page 2 `0x63` support)
- `src/gen/root_1a19.c` — combat `+Atk`/`+Def`, Extra Life (`L_0902`/`L_0b1b` returning `0xFFFF`), win XP
