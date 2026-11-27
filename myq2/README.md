# MyQ2 (Quake II for DOSBox-Pure)

Quake II (`Q2DOS`) compiled for 32-bit DOS via DJGPP with:
- **Classic Mode intact:** Full Single Player campaign and Classic Multiplayer (`baseq2/gamex86.dxe`)
- **Random Bot DM UI:** Built right into the menus (`Main Menu -> [R]` or `Multiplayer -> Random Bot DM`) using **ACE Bot** (`ace/gamex86.dxe`)
- **18 DM Maps (zero missing textures):**
  - **Official (8):** `q2dm1`–`q2dm8`
  - **Community Classics (10):** `aeroq2` (Aerowalk), `ztn2dm1` (Painkiller), `ztn2dm2` (The Killing Machine), `ztn2dm3` (The Rage), `ztn2dm4` (Recycler), `ztn2dm5` (The Crucible), `q2rdm1` (The Flesh Refinery), `monastery` (The Monastery), `ptrip` (POWERTRIP), `tltf` (The Longest Three Feet)
- **Randomization each match:**
  - Random map selection from All / Official / Community pool (auto-rotates to a new random map on match end)
  - Random bot count (`3–6` or fixed `2–8`) with randomized names & skins
  - **Item Shuffle:** Randomizes weapon, armor, and powerup pickups on each match
  - **Random Mutators:** Classic Arena, Low Gravity, Heavy Loadout, Rail & Rockets, Vampire (`+15 HP/Frag`), Armor Surge
- **Slow HP & Ammo Regen:** Enabled by default in both Random Bot DM and Classic mode (`myq2_hpregen 1`, `myq2_ammoregen 1`; configurable `off` / `slow` / `medium` in UI or console)
- **Accurate 3D Aiming** + pre-configured **WASD + Mouse Freelook** (`autoexec.cfg`)

## DOSBox-Pure Usage
1. Load `release/myq2_<VERSION>_dospure.zip` directly in **DOSBox-Pure** (recommend 64 MB RAM & Pentium/Max CPU speed, Direct Mouse mode).
2. `DOSBOX.BAT` auto-launches `Q2.EXE`.
3. From the Main Menu:
   - Press **`R`** (or go to **Multiplayer -> Random Bot DM**) to configure & launch a Random Bot DM match.
   - Or choose **Game** for Classic Quake II Single Player!
   - Press **`R`** in-game anytime to open the Random Bot DM menu and re-roll a new match.

## Building
```bash
chmod +x build.sh
./build.sh
```
Requires retail `pak0.pak` and `pak1.pak` (and optional `pak2.pak`) in `original_paks/`.

## Changelog
- **v1.3** — Zombie Survival polish & balance: added **Difficulty** (`Normal` / `Hard` / `Nightmare`) in the UI, rebalanced RPG upgrades (capped Vampiric Rounds at 25%, tuned Quad Surge to 8s/+4s, Arsenal upgrades grant +10% dmg if already owned), auto-reopen upgrade menu if dismissed, fixed CTF tech powerups spawning when `ctf=0`, fixed `%` display in centerprints; verified full 3-round + upgrade + lifesteal playthrough live in DOSBox-X.
- **v1.2** — Added **Zombie / Wave Survival (Roguelite)** mode (`[Z]` on Main Menu or `Multiplayer -> Zombie Survival`): picks a random compact arena (11 tight maps), escalating Strogg waves + Boss every 5th round, anti-stuck AI watchdog, built-in **Heal per Damage** (5% base, stackable), and 3-card Roguelite RPG upgrades between rounds (`[`/`]`+Enter or `F1`/`F2`/`F3`).
- **v1.1** — Fixed `ctf` defaulting to `1` in `ace/gamex86.dxe` (Random Bot DM now launches cleanly in Free-For-All Deathmatch with bots fighting everyone, verified in DOSBox-X).
- **v1.0** — Initial Q2DOS + ACE Bot build for DOSBox-Pure: Random Bot DM UI menu, 18-map pool (8 official + 10 verified stock-texture community classics), item shuffle, 6 random match mutators, slow HP & ammo regen, Classic SP/DM preserved.
